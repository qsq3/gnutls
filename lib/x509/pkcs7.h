
typedef struct gnutls_pkcs7_int {
	ASN1_TYPE pkcs7;
} gnutls_pkcs7_int;

typedef struct gnutls_pkcs7_int *gnutls_pkcs7_t;

int gnutls_pkcs7_init(gnutls_pkcs7_t * pkcs7);
void gnutls_pkcs7_deinit(gnutls_pkcs7_t pkcs7);
int gnutls_pkcs7_import(gnutls_pkcs7_t pkcs7, const gnutls_datum_t * data,
	gnutls_x509_crt_fmt_t format);
int gnutls_pkcs7_get_crt_raw(gnutls_pkcs7_t pkcs7, 
	int indx, void* certificate, size_t* certificate_size);
int gnutls_pkcs7_get_crt_count(gnutls_pkcs7_t pkcs7);
