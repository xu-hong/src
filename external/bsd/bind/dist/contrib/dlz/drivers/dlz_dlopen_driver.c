/*	$NetBSD: dlz_dlopen_driver.c,v 1.3 2011/09/11 18:55:32 christos Exp $	*/

/*
 * Copyright (C) 2010 Andrew Tridgell
 *
 * based on dlz_stub_driver.c
 * which is:
 * Copyright (C) 2002 Stichting NLnet, Netherlands, stichting@nlnet.nl.
 * Copyright (C) 1999-2001  Internet Software Consortium.
 * see dlz_stub_driver.c for details
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the
 * above copyright notice and this permission notice appear in all
 * copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR
 * DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL
 * STICHTING NLNET BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS
 * OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE
 * OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE
 * USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 * The development of Dynamically Loadable Zones (DLZ) for Bind 9 was
 * conceived and contributed by Rob Butler.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the
 * above copyright notice and this permission notice appear in all
 * copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND ROB BUTLER
 * DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL
 * ROB BUTLER BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS
 * OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE
 * OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE
 * USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#ifdef DLZ_DLOPEN

#include <config.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <dlfcn.h>

#include <dns/log.h>
#include <dns/sdlz.h>
#include <dns/result.h>

#include <isc/mem.h>
#include <isc/print.h>
#include <isc/result.h>
#include <isc/util.h>

#include <named/globals.h>

#include <dlz/dlz_dlopen_driver.h>

static dns_sdlzimplementation_t *dlz_dlopen = NULL;


typedef struct dlopen_data {
	isc_mem_t *mctx;
	char *dl_path;
	char *dlzname;
	void *dl_handle;
	void *dbdata;
	unsigned int flags;
	isc_mutex_t lock;
	int version;
	isc_boolean_t in_configure;

	int (*dlz_version)(unsigned int *flags);
	isc_result_t (*dlz_create)(const char *dlzname,
				   unsigned int argc, char *argv[],
				   void **dbdata, ...);
	isc_result_t (*dlz_findzonedb)(void *dbdata, const char *name);
	isc_result_t (*dlz_lookup)(const char *zone, const char *name,
				   void *dbdata, dns_sdlzlookup_t *lookup);
	isc_result_t (*dlz_authority)(const char *zone, void *dbdata,
				      dns_sdlzlookup_t *lookup);
	isc_result_t (*dlz_allnodes)(const char *zone, void *dbdata,
				     dns_sdlzallnodes_t *allnodes);
	isc_result_t (*dlz_allowzonexfr)(void *dbdata, const char *name,
					 const char *client);
	isc_result_t (*dlz_newversion)(const char *zone, void *dbdata,
				       void **versionp);
	void         (*dlz_closeversion)(const char *zone, isc_boolean_t commit,
					 void *dbdata, void **versionp);
	isc_result_t (*dlz_configure)(dns_view_t *view, void *dbdata);
	isc_boolean_t (*dlz_ssumatch)(const char *signer, const char *name,
				      const char *tcpaddr, const char *type,
				      const char *key, uint32_t keydatalen,
				      uint8_t *keydata, void *dbdata);
	isc_result_t (*dlz_addrdataset)(const char *name, const char *rdatastr,
					void *dbdata, void *version);
	isc_result_t (*dlz_subrdataset)(const char *name, const char *rdatastr,
					void *dbdata, void *version);
	isc_result_t (*dlz_delrdataset)(const char *name, const char *type,
					void *dbdata, void *version);
	void         (*dlz_destroy)(void *dbdata);
} dlopen_data_t;

/* Modules can choose whether they are lock-safe or not. */
#define MAYBE_LOCK(cd) \
	do { \
		if ((cd->flags & DNS_SDLZFLAG_THREADSAFE) == 0 && \
		    cd->in_configure == ISC_FALSE) \
			LOCK(&cd->lock); \
	} while (/*CONSTCOND*/0)

#define MAYBE_UNLOCK(cd) \
	do { \
		if ((cd->flags & DNS_SDLZFLAG_THREADSAFE) == 0 && \
		    cd->in_configure == ISC_FALSE) \
			UNLOCK(&cd->lock); \
	} while (/*CONSTCOND*/0)

/*
 * Log a message at the given level.
 */
static void dlopen_log(int level, const char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	isc_log_vwrite(dns_lctx, DNS_LOGCATEGORY_DATABASE,
		       DNS_LOGMODULE_DLZ, ISC_LOG_DEBUG(level),
		       fmt, ap);
	va_end(ap);
}

/*
 * SDLZ methods
 */

static isc_result_t
dlopen_dlz_allnodes(const char *zone, void *driverarg, void *dbdata,
		    dns_sdlzallnodes_t *allnodes)
{
	dlopen_data_t *cd = (dlopen_data_t *) dbdata;
	isc_result_t result;


	UNUSED(driverarg);

	if (cd->dlz_allnodes == NULL) {
		return (ISC_R_NOPERM);
	}

	MAYBE_LOCK(cd);
	result = cd->dlz_allnodes(zone, cd->dbdata, allnodes);
	MAYBE_UNLOCK(cd);
	return (result);
}


static isc_result_t
dlopen_dlz_allowzonexfr(void *driverarg, void *dbdata, const char *name,
		        const char *client)
{
	dlopen_data_t *cd = (dlopen_data_t *) dbdata;
	isc_result_t result;

	UNUSED(driverarg);


	if (cd->dlz_allowzonexfr == NULL) {
		return (ISC_R_NOPERM);
	}

	MAYBE_LOCK(cd);
	result = cd->dlz_allowzonexfr(cd->dbdata, name, client);
	MAYBE_UNLOCK(cd);
	return (result);
}

static isc_result_t
dlopen_dlz_authority(const char *zone, void *driverarg, void *dbdata,
		   dns_sdlzlookup_t *lookup)
{
	dlopen_data_t *cd = (dlopen_data_t *) dbdata;
	isc_result_t result;

	UNUSED(driverarg);

	if (cd->dlz_authority == NULL) {
		return (ISC_R_NOTIMPLEMENTED);
	}

	MAYBE_LOCK(cd);
	result = cd->dlz_authority(zone, cd->dbdata, lookup);
	MAYBE_UNLOCK(cd);
	return (result);
}

static isc_result_t
dlopen_dlz_findzonedb(void *driverarg, void *dbdata, const char *name)
{
	dlopen_data_t *cd = (dlopen_data_t *) dbdata;
	isc_result_t result;

	UNUSED(driverarg);

	MAYBE_LOCK(cd);
	result = cd->dlz_findzonedb(cd->dbdata, name);
	MAYBE_UNLOCK(cd);
	return (result);
}


static isc_result_t
dlopen_dlz_lookup(const char *zone, const char *name, void *driverarg,
		  void *dbdata, dns_sdlzlookup_t *lookup)
{
	dlopen_data_t *cd = (dlopen_data_t *) dbdata;
	isc_result_t result;

	UNUSED(driverarg);

	MAYBE_LOCK(cd);
	result = cd->dlz_lookup(zone, name, cd->dbdata, lookup);
	MAYBE_UNLOCK(cd);
	return (result);
}

/*
 * Load a symbol from the library
 */
static void *
dl_load_symbol(dlopen_data_t *cd, const char *symbol, bool mandatory) {
	void *ptr = dlsym(cd->dl_handle, symbol);
	if (ptr == NULL && mandatory) {
		dlopen_log(ISC_LOG_ERROR,
		           "dlz_dlopen: library '%s' is missing "
			   "required symbol '%s'", cd->dl_path, symbol);
	}
	return (ptr);
}

/*
 * Called at startup for each dlopen zone in named.conf
 */
static isc_result_t
dlopen_dlz_create(const char *dlzname, unsigned int argc, char *argv[],
		  void *driverarg, void **dbdata)
{
	dlopen_data_t *cd;
	isc_mem_t *mctx = NULL;
	isc_result_t result = ISC_R_FAILURE;
	int dlopen_flags;

	UNUSED(driverarg);

	if (argc < 2) {
		dlopen_log(ISC_LOG_ERROR,
			   "dlz_dlopen driver for '%s' needs a path to "
			   "the shared library", dlzname);
		return (ISC_R_FAILURE);
	}

	isc_mem_create(0, 0, &mctx);

	cd = isc_mem_get(mctx, sizeof(*cd));
	if (cd == NULL) {
		isc_mem_destroy(&mctx);
		return (ISC_R_NOMEMORY);
	}
	memset(cd, 0, sizeof(*cd));

	cd->mctx = mctx;

	cd->dl_path = isc_mem_strdup(cd->mctx, argv[1]);
	if (cd->dl_path == NULL) {
		goto failed;
	}

	cd->dlzname = isc_mem_strdup(cd->mctx, dlzname);
	if (cd->dlzname == NULL) {
		goto failed;
	}

	/* Open the library */
	dlopen_flags = RTLD_NOW;

#ifdef RTLD_DEEPBIND
	/*
	 * If RTLD_DEEPBIND is available then use it. This can avoid
	 * issues with a module using a different version of a system
	 * library than one that bind9 uses. For example, bind9 may link
	 * to MIT kerberos, but the module may use Heimdal. If we don't
	 * use RTLD_DEEPBIND then we could end up with Heimdal functions
	 * calling MIT functions, which leads to bizarre results (usually
	 * a segfault).
	 */
	dlopen_flags |= RTLD_DEEPBIND;
#endif

	cd->dl_handle = dlopen(cd->dl_path, dlopen_flags);
	if (cd->dl_handle == NULL) {
		dlopen_log(ISC_LOG_ERROR,
			   "dlz_dlopen failed to open library '%s' - %s",
			   cd->dl_path, dlerror());
		goto failed;
	}

	/* Find the symbols */
	cd->dlz_version      = dl_load_symbol(cd, "dlz_version", true);
	cd->dlz_create       = dl_load_symbol(cd, "dlz_create", true);
	cd->dlz_lookup       = dl_load_symbol(cd, "dlz_lookup", true);
	cd->dlz_findzonedb   = dl_load_symbol(cd, "dlz_findzonedb", true);

	if (cd->dlz_create == NULL || cd->dlz_lookup == NULL ||
	    cd->dlz_findzonedb == NULL)
	{
		/* We're missing a required symbol */
		goto failed;
	}

	cd->dlz_allowzonexfr = dl_load_symbol(cd, "dlz_allowzonexfr", false);
	cd->dlz_allnodes     = dl_load_symbol(cd, "dlz_allnodes",
					      cd->dlz_allowzonexfr != NULL);
	cd->dlz_authority    = dl_load_symbol(cd, "dlz_authority", false);
	cd->dlz_newversion   = dl_load_symbol(cd, "dlz_newversion", false);
	cd->dlz_closeversion = dl_load_symbol(cd, "dlz_closeversion",
					      cd->dlz_newversion != NULL);
	cd->dlz_configure    = dl_load_symbol(cd, "dlz_configure", false);
	cd->dlz_ssumatch     = dl_load_symbol(cd, "dlz_ssumatch", false);
	cd->dlz_addrdataset  = dl_load_symbol(cd, "dlz_addrdataset", false);
	cd->dlz_subrdataset  = dl_load_symbol(cd, "dlz_subrdataset", false);
	cd->dlz_delrdataset  = dl_load_symbol(cd, "dlz_delrdataset", false);

	/* Check the version of the API is the same */
	cd->version = cd->dlz_version(&cd->flags);
	if (cd->version != DLZ_DLOPEN_VERSION) {
		dlopen_log(ISC_LOG_ERROR,
			   "dlz_dlopen: incorrect version %d "
			   "should be %d in '%s'",
			   cd->version, DLZ_DLOPEN_VERSION, cd->dl_path);
		goto failed;
	}

	/*
	 * Call the library's create function. Note that this is an
	 * extended version of dlz create, with the addition of
	 * named function pointers for helper functions that the
	 * driver will need. This avoids the need for the backend to
	 * link the bind9 libraries
	 */
	MAYBE_LOCK(cd);
	result = cd->dlz_create(dlzname, argc-1, argv+1,
				&cd->dbdata,
				"log", dlopen_log,
				"putrr", dns_sdlz_putrr,
				"putnamedrr", dns_sdlz_putnamedrr,
				"writeable_zone", dns_dlz_writeablezone,
				NULL);
	MAYBE_UNLOCK(cd);
	if (result != ISC_R_SUCCESS)
		goto failed;

	*dbdata = cd;

	return (ISC_R_SUCCESS);

failed:
	dlopen_log(ISC_LOG_ERROR, "dlz_dlopen of '%s' failed", dlzname);
	if (cd->dl_path)
		isc_mem_free(mctx, cd->dl_path);
	if (cd->dlzname)
		isc_mem_free(mctx, cd->dlzname);
#ifdef HAVE_DLCLOSE
	if (cd->dl_handle)
		dlclose(cd->dl_handle);
#endif
	isc_mem_put(mctx, cd, sizeof(*cd));
	isc_mem_destroy(&mctx);
	return (result);
}


/*
 * Called when bind is shutting down
 */
static void
dlopen_dlz_destroy(void *driverarg, void *dbdata) {
	dlopen_data_t *cd = (dlopen_data_t *) dbdata;
	isc_mem_t *mctx;

	UNUSED(driverarg);

	if (cd->dlz_destroy) {
		MAYBE_LOCK(cd);
		cd->dlz_destroy(cd->dbdata);
		MAYBE_UNLOCK(cd);
	}

	if (cd->dl_path)
		isc_mem_free(cd->mctx, cd->dl_path);
	if (cd->dlzname)
		isc_mem_free(cd->mctx, cd->dlzname);
#ifdef HAVE_DLCLOSE
	if (cd->dl_handle)
		dlclose(cd->dl_handle);
#endif
	mctx = cd->mctx;
	isc_mem_put(mctx, cd, sizeof(*cd));
	isc_mem_destroy(&mctx);
}

/*
 * Called to start a transaction
 */
static isc_result_t
dlopen_dlz_newversion(const char *zone, void *driverarg, void *dbdata,
		      void **versionp)
{
	dlopen_data_t *cd = (dlopen_data_t *) dbdata;
	isc_result_t result;

	UNUSED(driverarg);

	if (cd->dlz_newversion == NULL)
		return (ISC_R_NOTIMPLEMENTED);

	MAYBE_LOCK(cd);
	result = cd->dlz_newversion(zone, cd->dbdata, versionp);
	MAYBE_UNLOCK(cd);
	return (result);
}

/*
 * Called to end a transaction
 */
static void
dlopen_dlz_closeversion(const char *zone, isc_boolean_t commit,
			void *driverarg, void *dbdata, void **versionp)
{
	dlopen_data_t *cd = (dlopen_data_t *) dbdata;

	UNUSED(driverarg);

	if (cd->dlz_newversion == NULL) {
		*versionp = NULL;
		return;
	}

	MAYBE_LOCK(cd);
	cd->dlz_closeversion(zone, commit, cd->dbdata, versionp);
	MAYBE_UNLOCK(cd);
}

/*
 * Called on startup to configure any writeable zones
 */
static isc_result_t
dlopen_dlz_configure(dns_view_t *view, void *driverarg, void *dbdata) {
	dlopen_data_t *cd = (dlopen_data_t *) dbdata;
	isc_result_t result;

	UNUSED(driverarg);

	if (cd->dlz_configure == NULL)
		return (ISC_R_SUCCESS);

	MAYBE_LOCK(cd);
	cd->in_configure = ISC_TRUE;
	result = cd->dlz_configure(view, cd->dbdata);
	cd->in_configure = ISC_FALSE;
	MAYBE_UNLOCK(cd);

	return (result);
}


/*
 * Check for authority to change a name
 */
static isc_boolean_t
dlopen_dlz_ssumatch(const char *signer, const char *name, const char *tcpaddr,
		    const char *type, const char *key, uint32_t keydatalen,
		    uint8_t *keydata, void *driverarg, void *dbdata)
{
	dlopen_data_t *cd = (dlopen_data_t *) dbdata;
	isc_boolean_t ret;

	UNUSED(driverarg);

	if (cd->dlz_ssumatch == NULL)
		return (ISC_FALSE);

	MAYBE_LOCK(cd);
	ret = cd->dlz_ssumatch(signer, name, tcpaddr, type, key, keydatalen,
			       keydata, cd->dbdata);
	MAYBE_UNLOCK(cd);

	return (ret);
}


/*
 * Add an rdataset
 */
static isc_result_t
dlopen_dlz_addrdataset(const char *name, const char *rdatastr,
		       void *driverarg, void *dbdata, void *version)
{
	dlopen_data_t *cd = (dlopen_data_t *) dbdata;
	isc_result_t result;

	UNUSED(driverarg);

	if (cd->dlz_addrdataset == NULL)
		return (ISC_R_NOTIMPLEMENTED);

	MAYBE_LOCK(cd);
	result = cd->dlz_addrdataset(name, rdatastr, cd->dbdata, version);
	MAYBE_UNLOCK(cd);

	return (result);
}

/*
 * Subtract an rdataset
 */
static isc_result_t
dlopen_dlz_subrdataset(const char *name, const char *rdatastr,
		       void *driverarg, void *dbdata, void *version)
{
	dlopen_data_t *cd = (dlopen_data_t *) dbdata;
	isc_result_t result;

	UNUSED(driverarg);

	if (cd->dlz_subrdataset == NULL)
		return (ISC_R_NOTIMPLEMENTED);

	MAYBE_LOCK(cd);
	result = cd->dlz_subrdataset(name, rdatastr, cd->dbdata, version);
	MAYBE_UNLOCK(cd);

	return (result);
}

/*
  delete a rdataset
 */
static isc_result_t
dlopen_dlz_delrdataset(const char *name, const char *type,
		       void *driverarg, void *dbdata, void *version)
{
	dlopen_data_t *cd = (dlopen_data_t *) dbdata;
	isc_result_t result;

	UNUSED(driverarg);

	if (cd->dlz_delrdataset == NULL)
		return (ISC_R_NOTIMPLEMENTED);

	MAYBE_LOCK(cd);
	result = cd->dlz_delrdataset(name, type, cd->dbdata, version);
	MAYBE_UNLOCK(cd);

	return (result);
}


static dns_sdlzmethods_t dlz_dlopen_methods = {
	dlopen_dlz_create,
	dlopen_dlz_destroy,
	dlopen_dlz_findzonedb,
	dlopen_dlz_lookup,
	dlopen_dlz_authority,
	dlopen_dlz_allnodes,
	dlopen_dlz_allowzonexfr,
	dlopen_dlz_newversion,
	dlopen_dlz_closeversion,
	dlopen_dlz_configure,
	dlopen_dlz_ssumatch,
	dlopen_dlz_addrdataset,
	dlopen_dlz_subrdataset,
	dlopen_dlz_delrdataset
};

/*
 * Register driver with BIND
 */
isc_result_t
dlz_dlopen_init(void) {
	isc_result_t result;

	dlopen_log(2, "Registering DLZ_dlopen driver");

	result = dns_sdlzregister("dlopen", &dlz_dlopen_methods, NULL,
				  DNS_SDLZFLAG_RELATIVEOWNER |
				  DNS_SDLZFLAG_THREADSAFE,
				  ns_g_mctx, &dlz_dlopen);

	if (result != ISC_R_SUCCESS) {
		UNEXPECTED_ERROR(__FILE__, __LINE__,
				 "dns_sdlzregister() failed: %s",
				 isc_result_totext(result));
		result = ISC_R_UNEXPECTED;
	}

	return (result);
}


/*
 * Unregister the driver
 */
void
dlz_dlopen_clear(void) {
	dlopen_log(2, "Unregistering DLZ_dlopen driver");
	if (dlz_dlopen != NULL)
		dns_sdlzunregister(&dlz_dlopen);
}

#endif
