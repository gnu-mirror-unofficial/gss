/* krb5/context.c --- Implementation of Kerberos 5 GSS Context functions.
 * Copyright (C) 2003, 2004  Simon Josefsson
 *
 * This file is part of the Generic Security Service (GSS).
 *
 * GSS is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * GSS is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public
 * License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with GSS; if not, write to the Free Software Foundation,
 * Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 *
 */

/* Get specification. */
#include "k5internal.h"

#define TOK_LEN 2
#define TOK_AP_REQ "\x01\x00"
#define TOK_AP_REP "\x02\x00"

OM_uint32
gss_krb5_init_sec_context (OM_uint32 * minor_status,
			   const gss_cred_id_t initiator_cred_handle,
			   gss_ctx_id_t * context_handle,
			   const gss_name_t target_name,
			   const gss_OID mech_type,
			   OM_uint32 req_flags,
			   OM_uint32 time_req,
			   const gss_channel_bindings_t input_chan_bindings,
			   const gss_buffer_t input_token,
			   gss_OID * actual_mech_type,
			   gss_buffer_t output_token,
			   OM_uint32 * ret_flags, OM_uint32 * time_rec)
{
  Shishi *h;
  Shishi_ap *ap;
  Shishi_tkt *tkt;
  char *data;
  size_t len;
  int rc;
  OM_uint32 maj_stat;

  if (!context_handle)
    return GSS_S_FAILURE;

  if (!output_token)
    return GSS_S_FAILURE;

  if (actual_mech_type)
    (*actual_mech_type) = GSS_KRB5;

  /* XXX mech_type not tested */

  if (*context_handle == GSS_C_NO_CONTEXT)
    {
      gss_ctx_id_t ctx;
      gss_buffer_desc tmp;

      ctx = xcalloc (sizeof (*ctx), 1);
      ctx->mech = GSS_KRB5;
      ctx->krb5 = xcalloc (sizeof (*ctx->krb5), 1);

      *context_handle = ctx;

      if (initiator_cred_handle)
	h = initiator_cred_handle->krb5->sh;
      else
	{
	  rc = shishi_init (&h);
	  if (rc != SHISHI_OK)
	    return GSS_S_FAILURE;
	}
      ctx->krb5->sh = h;

      if (gss_oid_equal (target_name->type, GSS_KRB5_NT_PRINCIPAL_NAME))
	{
	  maj_stat = gss_duplicate_name (minor_status, target_name,
					 &ctx->peerptr);
	}
      else
	{
	  maj_stat = gss_krb5_canonicalize_name (minor_status, target_name,
						 mech_type, &ctx->peerptr);
	}
      if (maj_stat != GSS_S_COMPLETE)
	return maj_stat;

      if (initiator_cred_handle)
	{
	  tkt = initiator_cred_handle->krb5->tkt;
	  /* XXX? */
	}
      else
	{
	  Shishi_tkts_hint hint;

	  memset (&hint, 0, sizeof (hint));
	  hint.server = malloc (ctx->peerptr->length + 1);
	  memcpy (hint.server, ctx->peerptr->value, ctx->peerptr->length);
	  hint.server[ctx->peerptr->length] = '\0';

	  tkt = shishi_tkts_get (shishi_tkts_default (h), &hint);
	  free (hint.server);
	  if (!tkt)
	    return GSS_S_FAILURE;

	  /* XXX */
	  shishi_tkts_to_file (shishi_tkts_default (h),
			       shishi_tkts_default_file (h));
	}
      ctx->krb5->tkt = tkt;

      data = xmalloc (2 + 24);
      memcpy (&data[0], TOK_AP_REQ, TOK_LEN);
      memcpy (&data[2], "\x10\x00\x00\x00", 4);	/* length of Bnd */
      memset (&data[6], 0, 16);	/* XXX we only support GSS_C_NO_BINDING */
      data[22] = req_flags & 0xFF;
      data[23] = (req_flags >> 8) & 0xFF;
      data[24] = (req_flags >> 16) & 0xFF;
      data[25] = (req_flags >> 24) & 0xFF;
      ctx->krb5->flags = req_flags;

      rc = shishi_ap_tktoptionsdata (h, &ap, tkt,
				     SHISHI_APOPTIONS_MUTUAL_REQUIRED, "a",
				     1);
      if (rc != SHISHI_OK)
	return GSS_S_FAILURE;
      (*context_handle)->krb5->ap = ap;

      ctx->krb5->key = shishi_ap_key (ap);

      rc = shishi_ap_req_build (ap);
      if (rc != SHISHI_OK)
	return GSS_S_FAILURE;

      rc = shishi_authenticator_set_cksum (h, shishi_ap_authenticator (ap),
					   0x8003, data + 2, 24);
      if (rc != SHISHI_OK)
	return GSS_S_FAILURE;

      rc = shishi_apreq_add_authenticator (h, shishi_ap_req (ap),
					   shishi_tkt_key (shishi_ap_tkt
							   (ap)),
					   SHISHI_KEYUSAGE_APREQ_AUTHENTICATOR,
					   shishi_ap_authenticator (ap));
      if (rc != SHISHI_OK)
	return GSS_S_FAILURE;

      free (data);

      rc = shishi_new_a2d (h, shishi_ap_req (ap), &data, &len);
      if (rc != SHISHI_OK)
	return GSS_S_FAILURE;

      tmp.length = len + TOK_LEN;
      tmp.value = xmalloc (tmp.length);
      memcpy (tmp.value, TOK_AP_REQ, TOK_LEN);
      memcpy ((char *) tmp.value + TOK_LEN, data, len);

      rc = gss_encapsulate_token (&tmp, GSS_KRB5, output_token);
      if (!rc)
	return GSS_S_FAILURE;

      if (req_flags & GSS_C_MUTUAL_FLAG)
	return GSS_S_CONTINUE_NEEDED;
      else
	return GSS_S_COMPLETE;
    }
  else if (*context_handle && !(*context_handle)->krb5->repdone)
    {
      gss_ctx_id_t ctx = *context_handle;
      _gss_krb5_ctx_t k5 = ctx->krb5;
      gss_OID_desc tokenoid;
      gss_buffer_desc data;

      rc = gss_decapsulate_token (input_token, &tokenoid, &data);
      if (!rc)
	return GSS_S_BAD_MIC;

      if (!gss_oid_equal (&tokenoid, GSS_KRB5))
	return GSS_S_BAD_MIC;

      if (memcmp (data.value, TOK_AP_REP, TOK_LEN) != 0)
	return GSS_S_BAD_MIC;

      rc = shishi_ap_rep_der_set (k5->ap, data.value + 2, data.length - 2);
      if (rc != SHISHI_OK)
	return GSS_S_FAILURE;

      rc = shishi_ap_rep_verify (k5->ap);
      if (rc != SHISHI_OK)
	return GSS_S_FAILURE;

      rc = shishi_encapreppart_seqnumber_get (k5->sh,
					      shishi_ap_encapreppart (k5->ap),
					      &k5->acceptseqnr);
      if (rc != SHISHI_OK)
	k5->acceptseqnr = 0;

      k5->repdone = 1;

      return GSS_S_COMPLETE;
    }

  return GSS_S_FAILURE;
}

OM_uint32
gss_krb5_accept_sec_context (OM_uint32 * minor_status,
			     gss_ctx_id_t * context_handle,
			     const gss_cred_id_t acceptor_cred_handle,
			     const gss_buffer_t input_token_buffer,
			     const gss_channel_bindings_t input_chan_bindings,
			     gss_name_t * src_name,
			     gss_OID * mech_type,
			     gss_buffer_t output_token,
			     OM_uint32 * ret_flags,
			     OM_uint32 * time_rec,
			     gss_cred_id_t * delegated_cred_handle)
{
  OM_uint32 maj_stat;
  int rc;

  if (minor_status)
    *minor_status = 0;

  if (mech_type)
    *mech_type = GSS_KRB5;

  if (ret_flags)
    *ret_flags = 0;

  if (!acceptor_cred_handle)
    /* XXX support GSS_C_NO_CREDENTIAL: acquire_cred() default server */
    return GSS_S_NO_CRED;

  if (input_chan_bindings != GSS_C_NO_CHANNEL_BINDINGS)
    /* XXX support channel bindings */
    return GSS_S_BAD_BINDINGS;

  if (!context_handle)
    return GSS_S_NO_CONTEXT;

  if (*context_handle)
    {
      printf ("bad\n");
    }
  else
    {
      gss_OID_desc tokenoid;
      gss_buffer_desc data;
      gss_ctx_id_t cx;
      _gss_krb5_ctx_t cxk5;
      _gss_krb5_cred_t crk5;
      Shishi_asn1 *p;

      crk5 = acceptor_cred_handle->krb5;

      cx = xcalloc (sizeof (*cx), 1);
      cxk5 = xcalloc (sizeof (*cxk5), 1);
      cx->mech = GSS_KRB5;
      cx->krb5 = cxk5;
      /* XXX cx->peer?? */
      *context_handle = cx;

      cxk5->sh = crk5->sh;
      cxk5->key = crk5->key;
      cxk5->acceptor = 1;

      rc = shishi_ap (cxk5->sh, &cxk5->ap);
      if (rc != SHISHI_OK)
	return GSS_S_FAILURE;

      rc = gss_decapsulate_token (input_token_buffer, &tokenoid, &data);
      if (!rc)
	return GSS_S_BAD_MIC;

      if (!gss_oid_equal (&tokenoid, GSS_KRB5))
	return GSS_S_BAD_MIC;

      if (memcmp (data.value, TOK_AP_REQ, TOK_LEN) != 0)
	return GSS_S_BAD_MIC;

      rc = shishi_ap_req_der_set (cxk5->ap, data.value + 2, data.length - 2);
      if (rc != SHISHI_OK)
	return GSS_S_FAILURE;

      rc = shishi_ap_req_process (cxk5->ap, crk5->key);
      if (rc != SHISHI_OK)
	return GSS_S_FAILURE;

      cxk5->tkt = shishi_ap_tkt (cxk5->ap);
      cxk5->key = shishi_tkt_key (cxk5->tkt);

      if (shishi_apreq_mutual_required_p (crk5->sh, shishi_ap_req (cxk5->ap)))
	{
	  Shishi_asn1 aprep;

	  rc = shishi_ap_rep_asn1 (cxk5->ap, &aprep);
	  if (rc != SHISHI_OK)
	    {
	      printf ("Error creating AP-REP: %s\n", shishi_strerror (rc));
	      return GSS_S_FAILURE;
	    }

	  rc = shishi_new_a2d (crk5->sh, aprep,
			       (char **) &data.value, &data.length);
	  if (rc != SHISHI_OK)
	    {
	      printf ("Error der encoding aprep: %s\n", shishi_strerror (rc));
	      return GSS_S_FAILURE;
	    }

	  rc = gss_encapsulate_token_prefix (&data, TOK_AP_REP, TOK_LEN,
					     GSS_KRB5, output_token);
	  if (!rc)
	    return GSS_S_FAILURE;

	  if (ret_flags)
	    *ret_flags = GSS_C_MUTUAL_FLAG;
	}
      else
	{
	  output_token->value = NULL;
	  output_token->length = 0;
	}

      if (src_name)
	{
	  gss_name_t p;

	  p = xmalloc (sizeof (*p));
	  p->length = 1024;	/* XXX */
	  p->value = xmalloc (p->length);

	  rc = shishi_encticketpart_cname_get
	    (cxk5->sh, shishi_tkt_encticketpart (cxk5->tkt),
	     p->value, &p->length);
	  if (rc != SHISHI_OK)
	    return GSS_S_FAILURE;

	  maj_stat = gss_duplicate_oid (minor_status,
					GSS_KRB5_NT_PRINCIPAL_NAME, &p->type);
	  if (GSS_ERROR (maj_stat))
	    return GSS_S_FAILURE;

	  *src_name = p;
	}
    }

  return GSS_S_COMPLETE;
}

OM_uint32
gss_krb5_delete_sec_context (OM_uint32 * minor_status,
			     gss_ctx_id_t * context_handle,
			     gss_buffer_t output_token)
{
  _gss_krb5_ctx_t k5 = (*context_handle)->krb5;

  if (minor_status)
    *minor_status = 0;

  if (k5->key)
    shishi_key_done (k5->key);

  return GSS_S_COMPLETE;
}

OM_uint32
gss_krb5_context_time (OM_uint32 * minor_status,
		       const gss_ctx_id_t context_handle,
		       OM_uint32 * time_rec)
{
  _gss_krb5_ctx_t k5 = context_handle->krb5;

  if (minor_status)
    *minor_status = 0;

  if (time_rec)
    {
      *time_rec = gss_krb5_tktlifetime (k5->tkt);

      if (*time_rec == 0)
	return GSS_S_CONTEXT_EXPIRED;
    }

  return GSS_S_COMPLETE;
}