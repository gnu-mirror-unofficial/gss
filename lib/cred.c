/* cred.c	Implementation of GSS-API Credential Management functions.
 * Copyright (C) 2003  Simon Josefsson
 *
 * This file is part of GPL GSS-API.
 *
 * GPL GSS-API is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * GPL GSS-API is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with GPL GSS-API; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#include "internal.h"

OM_uint32
gss_acquire_cred (OM_uint32 * minor_status,
		  const gss_name_t desired_name,
		  OM_uint32 time_req,
		  const gss_OID_set desired_mechs,
		  gss_cred_usage_t cred_usage,
		  gss_cred_id_t * output_cred_handle,
		  gss_OID_set * actual_mechs, OM_uint32 * time_rec)
{
  return GSS_S_FAILURE;
}

OM_uint32
gss_add_cred (OM_uint32 * minor_status,
	      const gss_cred_id_t input_cred_handle,
	      const gss_name_t desired_name,
	      const gss_OID desired_mech,
	      gss_cred_usage_t cred_usage,
	      OM_uint32 initiator_time_req,
	      OM_uint32 acceptor_time_req,
	      gss_cred_id_t * output_cred_handle,
	      gss_OID_set * actual_mechs,
	      OM_uint32 * initiator_time_rec, OM_uint32 * acceptor_time_rec)
{
  return GSS_S_FAILURE;
}

OM_uint32
gss_inquire_cred (OM_uint32 * minor_status,
		  const gss_cred_id_t cred_handle,
		  gss_name_t * name,
		  OM_uint32 * lifetime,
		  gss_cred_usage_t * cred_usage, gss_OID_set * mechanisms)
{
  return GSS_S_FAILURE;
}

OM_uint32
gss_inquire_cred_by_mech (OM_uint32 * minor_status,
			  const gss_cred_id_t cred_handle,
			  const gss_OID mech_type,
			  gss_name_t * name,
			  OM_uint32 * initiator_lifetime,
			  OM_uint32 * acceptor_lifetime,
			  gss_cred_usage_t * cred_usage)
{
  return GSS_S_FAILURE;
}

/**
 * gss_release_cred:
 * @minor_status: Mechanism specific status code.
 * @cred_handle: Optional opaque handle identifying credential to be
 *   released.  If GSS_C_NO_CREDENTIAL is supplied, the routine will
 *   complete successfully, but will do nothing.
 *
 * Informs GSS-API that the specified credential handle is no longer
 * required by the application, and frees associated resources.
 * Implementations are encouraged to set the cred_handle to
 * GSS_C_NO_CREDENTIAL on successful completion of this call.
 *
 * Return value: Returns GSS_S_COMPLETE for successful completion, and
 *   GSS_S_NO_CRED for credentials could not be accessed.
 **/
OM_uint32
gss_release_cred (OM_uint32 * minor_status, gss_cred_id_t * cred_handle)
{
  if (cred_handle && *cred_handle)
    free (*cred_handle);

  *cred_handle = GSS_C_NO_CREDENTIAL;

  if (minor_status)
    minor_status = 0;
  return GSS_S_COMPLETE;
}