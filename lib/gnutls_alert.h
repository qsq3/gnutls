typedef enum gnutls_alert_level { GNUTLS_AL_WARNING=1, GNUTLS_AL_FATAL 
} gnutls_alert_level;

typedef enum AlertDescription { 
	GNUTLS_A_CLOSE_NOTIFY, GNUTLS_A_UNEXPECTED_MESSAGE=10, 
	GNUTLS_A_BAD_RECORD_MAC=20, GNUTLS_A_DECRYPTION_FAILED, 
	GNUTLS_A_RECORD_OVERFLOW, GNUTLS_A_DECOMPRESSION_FAILURE=30,
	GNUTLS_A_HANDSHAKE_FAILURE=40, GNUTLS_A_SSL3_NO_CERTIFICATE=41,
	GNUTLS_A_BAD_CERTIFICATE=42, GNUTLS_A_UNSUPPORTED_CERTIFICATE,
	GNUTLS_A_CERTIFICATE_REVOKED, GNUTLS_A_CERTIFICATE_EXPIRED, 
	GNUTLS_A_CERTIFICATE_UNKNOWN, GNUTLS_A_ILLEGAL_PARAMETER, 
	GNUTLS_A_UNKNOWN_CA, GNUTLS_A_ACCESS_DENIED, GNUTLS_A_DECODE_ERROR=50,
	GNUTLS_A_DECRYPT_ERROR, GNUTLS_A_EXPORT_RESTRICTION=60, 
	GNUTLS_A_PROTOCOL_VERSION=70, GNUTLS_A_INSUFFICIENT_SECURITY, 
	GNUTLS_A_INTERNAL_ERROR=80, GNUTLS_A_USER_CANCELED=90,
	GNUTLS_A_NO_RENEGOTIATION=100, GNUTLS_A_UNSUPPORTED_EXTENSION=110,
	GNUTLS_A_CERTIFICATE_UNOBTAINABLE=111, GNUTLS_A_UNRECOGNIZED_NAME=112,
	GNUTLS_A_UNKNOWN_SRP_USERNAME=120, GNUTLS_A_MISSING_SRP_USERNAME=121
} gnutls_alert_description;

gnutls_alert_description gnutls_alert_get( gnutls_session session);
int gnutls_alert_send( gnutls_session session, gnutls_alert_level level, gnutls_alert_description desc);
const char* gnutls_alert_get_name( gnutls_alert_level alert);
