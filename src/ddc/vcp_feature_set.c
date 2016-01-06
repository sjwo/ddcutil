/* vcp_feature_groups.c
 *
 * Created on: Dec 29, 2015
 *     Author: rock
 *
 * <copyright>
 * Copyright (C) 2014-2015 Sanford Rockowitz <rockowitz@minsoft.com>
 *
 * Licensed under the GNU General Public License Version 2
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 * </endcopyright>
 */

#include <assert.h>
#include <glib.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "util/report_util.h"

#include "base/msg_control.h"

#include "ddc/vcp_feature_set.h"


#define VCP_FEATURE_SET_MARKER "FSET"
struct VCP_Feature_Set {
   char                marker[4];
   VCP_Feature_Subset  subset;
   GPtrArray *         members;
};


VCP_Feature_Set
create_feature_set(VCP_Feature_Subset subset, Version_Spec vcp_version) {
   // bool debug = false;
   struct VCP_Feature_Set * fset = calloc(1,sizeof(struct VCP_Feature_Set));
   memcpy(fset->marker, VCP_FEATURE_SET_MARKER, 4);
   fset->subset = subset;
   fset->members = g_ptr_array_sized_new(30);
   if (subset == SUBSET_SCAN) {
      int ndx = 0;
      for (ndx = 0; ndx < 256; ndx++) {
         Byte id = ndx;
         VCP_Feature_Table_Entry* vcp_entry = vcp_find_feature_by_hexid_w_default(id);
         // original code looks at VCP2_READABLE, output level
         g_ptr_array_add(fset->members, vcp_entry);
      }
   }
   else {
      int known_feature_ct = vcp_get_feature_code_count();
      int ndx = 0;
      for (ndx=0; ndx < known_feature_ct; ndx++) {
         VCP_Feature_Table_Entry * vcp_entry = vcp_get_feature_table_entry(ndx);
         assert(vcp_entry);
         Version_Feature_Flags vflags = 0;
         bool showit = false;
         switch(subset) {
         case SUBSET_ALL:
            showit = true;
            break;
         case SUBSET_SUPPORTED:
            showit = true;
            break;
         case SUBSET_COLORMGT:
            vflags = get_version_specific_feature_flags(vcp_entry, vcp_version);;
            showit = (vflags & VCP_SUBSET_COLOR) | (vcp_entry->vcp_subsets & VCP_SUBSET_COLOR);
            break;
         case SUBSET_PROFILE:
            vflags = get_version_specific_feature_flags(vcp_entry, vcp_version);;
            showit = (vflags & VCP_SUBSET_PROFILE) | (vcp_entry->vcp_subsets & VCP_SUBSET_PROFILE);
            break;
         case SUBSET_SCAN:    // will never happen, inserted to avoid compiler warning
         case SUBSET_SINGLE_FEATURE:
            break;
         }
         if (showit) {
            g_ptr_array_add(fset->members, vcp_entry);
         }
      }
   }

   return fset;
}



VCP_Feature_Set
create_single_feature_set_by_vcp_entry(VCP_Feature_Table_Entry * vcp_entry) {
   // bool debug = true;
   struct VCP_Feature_Set * fset = calloc(1,sizeof(struct VCP_Feature_Set));
   memcpy(fset->marker, VCP_FEATURE_SET_MARKER, 4);
   fset->members = g_ptr_array_sized_new(1);
   fset->subset = SUBSET_SINGLE_FEATURE;
   g_ptr_array_add(fset->members, vcp_entry);
   return fset;
}

/* Creates a VCP_Feature_Set for a single VCP code
 *
 * Arguments:
 *    id      feature id
 *    force   indicates behavior if feature id not found in vcp_feature_table,
 *            if true, creates a feature set using a dummy feature table entry
 *            if false, returns NULL
 *
 * Returns: feature set containing a single feature
 *          NULL if the feature not found and force not specified
 */
VCP_Feature_Set
create_single_feature_set_by_hexid(Byte id, bool force) {
   struct VCP_Feature_Set * fset = NULL;
   VCP_Feature_Table_Entry* vcp_entry = NULL;
   if (force)
      vcp_entry = vcp_find_feature_by_hexid_w_default(id);
   else
      vcp_entry = vcp_find_feature_by_hexid(id);
   if (vcp_entry)
      fset = create_single_feature_set_by_vcp_entry(vcp_entry);
   return fset;
}


/* Creates a VCP_Feature_Set from an external feature specification
 *
 * Arguments:
 *    fsref   external feature set descriptor
 *    force   indicates behavior in the case of a single feature code
 *            if the feature id is not found in vcp_feature_table,
 *            if true, creates a feature set using a dummy feature table entry
 *            if false, returns NULL
 *
 * Returns: feature set containing a single feature
 *          NULL if the feature not found and force not specified
 */
VCP_Feature_Set
create_feature_set_from_feature_set_ref(
   Feature_Set_Ref * fsref,
   Version_Spec      vcp_version,
   bool              force)
{
    struct VCP_Feature_Set * fset = NULL;
    if (fsref->subset == SUBSET_SINGLE_FEATURE)
       fset = create_single_feature_set_by_hexid(fsref->specific_feature, force);
    else
       fset = create_feature_set(fsref->subset, vcp_version);
    return fset;
}


VCP_Feature_Set create_single_feature_set_by_charid(Byte id, bool force) {
   // TODO: copy and modify existing code:
   return NULL;
}

static inline struct VCP_Feature_Set *
unopaque_feature_set(VCP_Feature_Set feature_set) {
   struct VCP_Feature_Set * fset = (struct VCP_Feature_Set *) feature_set;
   assert( fset && memcmp(fset->marker, VCP_FEATURE_SET_MARKER, 4) == 0);
   return fset;
}


void free_feature_set(VCP_Feature_Set feature_set) {
   struct VCP_Feature_Set * fset = (struct VCP_Feature_Set *) feature_set;
   assert( fset && memcmp(fset->marker, VCP_FEATURE_SET_MARKER, 4) == 0);
   int ndx = 0;
   // free all generated members
   for (; ndx < fset->members->len; ndx++) {
      VCP_Feature_Table_Entry * vcp_entry = NULL;
      vcp_entry = g_ptr_array_index(fset->members,ndx);
      if (vcp_entry->vcp_global_flags & VCP2_SYNTHETIC) {
         // free_vcp_feature_table_entry(vcp_entry);    // UNIMPLEMENTED
      }
   }
   fset->marker[3] = 'x';
   free(fset);
}

VCP_Feature_Table_Entry * get_feature_set_entry(VCP_Feature_Set feature_set, int index) {
   struct VCP_Feature_Set * fset = (struct VCP_Feature_Set *) feature_set;
   assert( fset && memcmp(fset->marker, VCP_FEATURE_SET_MARKER, 4) == 0);
   VCP_Feature_Table_Entry * ventry = NULL;
   if (index >= 0 || index < fset->members->len)
      ventry = g_ptr_array_index(fset->members,index);
   return ventry;
}

int get_feature_set_size(VCP_Feature_Set feature_set) {
   struct VCP_Feature_Set * fset = (struct VCP_Feature_Set *) feature_set;
   assert( fset && memcmp(fset->marker, VCP_FEATURE_SET_MARKER, 4) == 0);
   return fset->members->len;
}

VCP_Feature_Subset get_feature_set_subset_id(VCP_Feature_Set feature_set) {
   struct VCP_Feature_Set * fset = (struct VCP_Feature_Set *) feature_set;
   assert( fset && memcmp(fset->marker, VCP_FEATURE_SET_MARKER, 4) == 0);
   return fset->subset;
}

void report_feature_set(VCP_Feature_Set feature_set, int depth) {
   struct VCP_Feature_Set * fset = (struct VCP_Feature_Set *) feature_set;
   assert( fset && memcmp(fset->marker, VCP_FEATURE_SET_MARKER, 4) == 0);
   int ndx = 0;
   // free all generated members
   for (; ndx < fset->members->len; ndx++) {
      VCP_Feature_Table_Entry * vcp_entry = NULL;
      vcp_entry = g_ptr_array_index(fset->members,ndx);
      rpt_vstring(depth,
                  "VCP code: %02X: %s",
                  vcp_entry->code,
                  get_non_version_specific_feature_name(vcp_entry)
                 );
   }
}
