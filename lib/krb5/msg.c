/* krb5/msg.c --- Implementation of Kerberos 5 GSS-API Per-Message functions.
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
#define TOK_WRAP   "\x02\x01"

#define C2I(buf) ((buf[0] & 0xFF) |		\
		  ((buf[1] & 0xFF) << 8) |	\
		  ((buf[2] & 0xFF) << 16) |	\
		  ((buf[3] & 0xFF) << 24))

OM_uint32
gss_krb5_wrap (OM_uint32 * minor_status,
	       const gss_ctx_id_t context_handle,
	       int conf_req_flag,
	       gss_qop_t qop_req,
	       const gss_buffer_t input_message_buffer,
	       int *conf_state, gss_buffer_t output_message_buffer)
{
  _gss_krb5_ctx_t k5 = context_handle->krb5;
  size_t padlength;
  gss_buffer_desc data;
  size_t tmplen;
  int rc;

  switch (shishi_key_type (k5->key))
    {
      /* XXX implement other checksums */

    case SHISHI_DES_CBC_MD5:
      {
	char header[8];
	char seqno[8];
	char *eseqno;
	char *cksum;
	char confounder[8];

	/* Typical data:
	   ;; 02 01 00 00 ff ff ff ff  0c 22 1f 79 59 3d 00 cb
	   ;; d5 78 2f fb 50 d2 b8 59  fb b4 e0 9b d0 a2 fa dc
	   ;; 01 00 20 00 04 04 04 04
	   Translates into:
	   ;;   HEADER                 ENCRYPTED SEQ.NUMBER
	   ;;   DES-MAC-MD5 CKSUM      CONFOUNDER
	   ;;   PADDED DATA
	 */
	padlength = 8 - input_message_buffer->length % 8;
	data.length = 4 * 8 + input_message_buffer->length + padlength;
	data.value = xmalloc (data.length);

	/* XXX encrypt data iff confidential option chosen */

	/* Setup header and confounder */
	memcpy (header, TOK_WRAP, 2);	/* TOK_ID: Wrap 0201 */
	memcpy (header + 2, "\x00\x00", 2);	/* SGN_ALG: DES-MAC-MD5 */
	memcpy (header + 4, "\xFF\xFF", 2);	/* SEAL_ALG: none */
	memcpy (header + 6, "\xFF\xFF", 2);	/* filler */
	rc = shishi_randomize (k5->sh, 0, confounder, 8);
	if (rc != SHISHI_OK)
	  return GSS_S_FAILURE;

	/* Compute checksum over header, confounder, input string, and pad */
	memcpy (data.value, header, 8);
	memcpy (data.value + 8, confounder, 8);
	memcpy (data.value + 16, input_message_buffer->value,
		input_message_buffer->length);
	memset (data.value + 16 + input_message_buffer->length,
		padlength, padlength);

	rc = shishi_checksum (k5->sh,
			      k5->key,
			      0, SHISHI_RSA_MD5_DES_GSS,
			      data.value,
			      16 + input_message_buffer->length + padlength,
			      &cksum, &tmplen);
	if (rc != SHISHI_OK || tmplen != 8)
	  return GSS_S_FAILURE;

	/* seq_nr */
	seqno[0] = k5->initseqnr & 0xFF;
	seqno[1] = k5->initseqnr >> 8 & 0xFF;
	seqno[2] = k5->initseqnr >> 16 & 0xFF;
	seqno[3] = k5->initseqnr >> 24 & 0xFF;
	memset (seqno + 4, k5->acceptor ? 0xFF : 0, 4);

	rc = shishi_encrypt_iv_etype (k5->sh, k5->key, 0, SHISHI_DES_CBC_NONE, cksum, 8,	/* cksum */
				      seqno, 8, &eseqno, &tmplen);
	if (rc != SHISHI_OK || tmplen != 8)
	  return GSS_S_FAILURE;

	/* put things in place */
	memcpy (data.value, header, 8);
	memcpy (data.value + 8, eseqno, 8);
	free (eseqno);
	memcpy (data.value + 16, cksum, 8);
	free (cksum);
	memcpy (data.value + 24, confounder, 8);
	memcpy (data.value + 32, input_message_buffer->value,
		input_message_buffer->length);
	memset (data.value + 32 + input_message_buffer->length,
		padlength, padlength);

	rc = gss_encapsulate_token (&data, GSS_KRB5, output_message_buffer);
	if (!rc)
	  return GSS_S_FAILURE;
	k5->initseqnr++;
      }
      break;

    case SHISHI_DES3_CBC_HMAC_SHA1_KD:
      {
	char *tmp;

	padlength = 8 - input_message_buffer->length % 8;
	data.length =
	  8 + 8 + 20 + 8 + input_message_buffer->length + padlength;
	data.value = xmalloc (data.length);

	/* XXX encrypt data iff confidential option chosen */

	/* Compute checksum over header, confounder, input string, and pad */

	memcpy (data.value, TOK_WRAP, 2);	/* TOK_ID: Wrap */
	memcpy (data.value + 2, "\x04\x00", 2);	/* SGN_ALG: 3DES */
	memcpy (data.value + 4, "\xFF\xFF", 2);	/* SEAL_ALG: none */
	memcpy (data.value + 6, "\xFF\xFF", 2);	/* filler */
	rc = shishi_randomize (k5->sh, 0, data.value + 8, 8);
	if (rc != SHISHI_OK)
	  return GSS_S_FAILURE;
	memcpy (data.value + 16, input_message_buffer->value,
		input_message_buffer->length);
	memset (data.value + 16 + input_message_buffer->length,
		padlength, padlength);

	rc = shishi_checksum (k5->sh,
			      k5->key,
			      SHISHI_KEYUSAGE_GSS_R2,
			      SHISHI_HMAC_SHA1_DES3_KD, data.value,
			      16 + input_message_buffer->length + padlength,
			      &tmp, &tmplen);
	if (rc != SHISHI_OK || tmplen != 20)
	  return GSS_S_FAILURE;

	memcpy (data.value + 16, tmp, tmplen);
	memcpy (data.value + 36, data.value + 8, 8);

	/* seq_nr */
	((char *) data.value + 8)[0] = k5->initseqnr & 0xFF;
	((char *) data.value + 8)[1] = k5->initseqnr >> 8 & 0xFF;
	((char *) data.value + 8)[2] = k5->initseqnr >> 16 & 0xFF;
	((char *) data.value + 8)[3] = k5->initseqnr >> 24 & 0xFF;
	memset (data.value + 8 + 4, k5->acceptor ? 0xFF : 0, 4);

	rc = shishi_encrypt_iv_etype (k5->sh, k5->key, 0, SHISHI_DES3_CBC_NONE, data.value + 16, 8,	/* cksum */
				      data.value + 8, 8, &tmp, &tmplen);
	if (rc != SHISHI_OK || tmplen != 8)
	  return GSS_S_FAILURE;

	memcpy (data.value + 8, tmp, tmplen);
	free (tmp);
	memcpy (data.value + 8 + 8 + 20 + 8, input_message_buffer->value,
		input_message_buffer->length);
	memset (data.value + 8 + 8 + 20 + 8 + input_message_buffer->length,
		padlength, padlength);

	rc = gss_encapsulate_token (&data, GSS_KRB5, output_message_buffer);
	if (!rc)
	  return GSS_S_FAILURE;
	k5->initseqnr++;
	break;
      }

    default:
      return GSS_S_FAILURE;
    }

  return GSS_S_COMPLETE;
}

OM_uint32
gss_krb5_unwrap (OM_uint32 * minor_status,
		 const gss_ctx_id_t context_handle,
		 const gss_buffer_t input_message_buffer,
		 gss_buffer_t output_message_buffer,
		 int *conf_state, gss_qop_t * qop_state)
{
  _gss_krb5_ctx_t k5 = context_handle->krb5;
  gss_OID_desc tokenoid;
  gss_buffer_desc data;
  OM_uint32 sgn_alg, seal_alg;
  size_t tmplen;
  int rc;

  rc = gss_decapsulate_token (input_message_buffer, &tokenoid, &data);
  if (!rc)
    return GSS_S_BAD_MIC;

  if (!gss_oid_equal (&tokenoid, GSS_KRB5))
    return GSS_S_BAD_MIC;

  if (data.length < 8)
    return GSS_S_BAD_MIC;

  if (memcmp (data.value, TOK_WRAP, TOK_LEN) != 0)
    return GSS_S_BAD_MIC;

  sgn_alg = ((char *) data.value)[2] & 0xFF;
  sgn_alg |= ((char *) data.value)[3] << 8 & 0xFF00;

  seal_alg = ((char *) data.value)[4] & 0xFF;
  seal_alg |= ((char *) data.value)[5] << 8 & 0xFF00;

  if (conf_state != NULL)
    *conf_state = seal_alg == 0xFFFF;

  if (memcmp (data.value + 6, "\xFF\xFF", 2) != 0)
    return GSS_S_BAD_MIC;

  switch (sgn_alg)
    {
      /* XXX implement other checksums */

    case 0:			/* DES-MD5 */
      {
	size_t padlen;
	unsigned char *pt;
	char header[8];
	char encseqno[8];
	char seqno[8];
	char cksum[8];
	char confounder[8];
	char *tmp;
	int seqnr;
	int i;

	/* Typical data:
	   ;; 02 01 00 00 ff ff ff ff  0c 22 1f 79 59 3d 00 cb
	   ;; d5 78 2f fb 50 d2 b8 59  fb b4 e0 9b d0 a2 fa dc
	   ;; 01 00 20 00 04 04 04 04
	   Translates into:
	   ;;   HEADER                 ENCRYPTED SEQ.NUMBER
	   ;;   DES-MAC-MD5 CKSUM      CONFOUNDER
	   ;;   PADDED DATA
	 */

	if (data.length < 5 * 8)
	  return GSS_S_BAD_MIC;

	memcpy (header, data.value, 8);
	memcpy (encseqno, data.value + 8, 8);
	memcpy (cksum, data.value + 16, 8);
	memcpy (confounder, data.value + 24, 8);
	pt = data.value + 32;

	/* XXX decrypt data iff confidential option chosen */

	rc = shishi_decrypt_iv_etype (k5->sh,
				      k5->key,
				      0, SHISHI_DES_CBC_NONE,
				      cksum, 8, encseqno, 8, &tmp, &i);
	if (rc != SHISHI_OK)
	  return GSS_S_FAILURE;
	if (i != 8)
	  return GSS_S_BAD_MIC;
	memcpy (seqno, tmp, 8);
	free (tmp);

	if (memcmp (seqno + 4, k5->acceptor ? "\x00\x00\x00\x00" :
		    "\xFF\xFF\xFF\xFF", 4) != 0)
	  return GSS_S_BAD_MIC;

	seqnr = C2I (seqno);
	if (seqnr != k5->acceptseqnr)
	  return GSS_S_BAD_MIC;

	k5->acceptseqnr++;

	/* Check pad */
	padlen = ((char *) data.value)[data.length - 1];
	if (padlen > 8)
	  return GSS_S_BAD_MIC;
	for (i = 1; i <= padlen; i++)
	  if (((char *) data.value)[data.length - i] != padlen)
	    return GSS_S_BAD_MIC;

	/* Write header and confounder next to data */
	memcpy (data.value + 16, header, 8);
	memcpy (data.value + 24, confounder, 8);

	/* Checksum header + confounder + data + pad */
	rc = shishi_checksum (k5->sh,
			      k5->key,
			      0, SHISHI_RSA_MD5_DES_GSS,
			      data.value + 16, data.length - 16,
			      &tmp, &tmplen);
	if (rc != SHISHI_OK || tmplen != 8)
	  return GSS_S_FAILURE;

	memcpy (data.value + 8, tmp, tmplen);

	/* Compare checksum */
	if (tmplen != 8 || memcmp (cksum, data.value + 8, 8) != 0)
	  return GSS_S_BAD_MIC;

	/* Copy output data */
	output_message_buffer->length = data.length - 8 - 8 - 8 - 8 - padlen;
	output_message_buffer->value =
	  xmalloc (output_message_buffer->length);
	memcpy (output_message_buffer->value, pt,
		data.length - 4 * 8 - padlen);
      }
      break;

    case 4:			/* 3DES */
      {
	size_t padlen;
	unsigned char *p;
	char *t;
	char cksum[20];
	int i;

	if (data.length < 8 + 8 + 20 + 8 + 8)
	  return GSS_S_BAD_MIC;

	memcpy (cksum, data.value + 8 + 8, 20);

	/* XXX decrypt data iff confidential option chosen */

	p = data.value + 8;
	rc = shishi_decrypt_iv_etype (k5->sh,
				      k5->key,
				      0, SHISHI_DES3_CBC_NONE,
				      cksum, 8, p, 8, &t, &i);
	if (rc != SHISHI_OK || i != 8)
	  return GSS_S_FAILURE;

	memcpy (p, t, i);
	free (t);

	if (memcmp (p + 4, k5->acceptor ? "\x00\x00\x00\x00" :
		    "\xFF\xFF\xFF\xFF", 4) != 0)
	  return GSS_S_BAD_MIC;
	if (C2I (p) != k5->acceptseqnr)
	  return GSS_S_BAD_MIC;

	k5->acceptseqnr++;

	/* Check pad */
	padlen = ((char *) data.value)[data.length - 1];
	if (padlen > 8)
	  return GSS_S_BAD_MIC;
	for (i = 1; i <= padlen; i++)
	  if (((char *) data.value)[data.length - i] != padlen)
	    return GSS_S_BAD_MIC;

	/* Write header next to confounder */
	memcpy (data.value + 8 + 20, data.value, 8);

	/* Checksum header + confounder + data + pad */
	rc = shishi_checksum (k5->sh,
			      k5->key,
			      SHISHI_KEYUSAGE_GSS_R2,
			      SHISHI_HMAC_SHA1_DES3_KD, data.value + 20 + 8,
			      data.length - 20 - 8, &t, &tmplen);
	if (rc != SHISHI_OK || tmplen != 20)
	  return GSS_S_FAILURE;

	memcpy (data.value + 8 + 8, t, tmplen);
	free (t);

	/* Compare checksum */
	if (tmplen != 20 || memcmp (cksum, data.value + 8 + 8, 20) != 0)
	  return GSS_S_BAD_MIC;

	/* Copy output data */
	output_message_buffer->length = data.length - 8 - 20 - 8 - 8 - padlen;
	output_message_buffer->value =
	  xmalloc (output_message_buffer->length);
	memcpy (output_message_buffer->value, data.value + 20 + 8 + 8 + 8,
		data.length - 20 - 8 - 8 - 8 - padlen);
      }
      break;

    default:
      return GSS_S_FAILURE;
    }

  return GSS_S_COMPLETE;
}
