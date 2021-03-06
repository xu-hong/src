/*	$NetBSD: config_reg.c,v 1.1.1.1 2011/04/13 18:15:32 elric Exp $	*/

/***********************************************************************
 * Copyright (c) 2010, Secure Endpoints Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * - Redistributions of source code must retain the above copyright
 *   notice, this list of conditions and the following disclaimer.
 *
 * - Redistributions in binary form must reproduce the above copyright
 *   notice, this list of conditions and the following disclaimer in
 *   the documentation and/or other materials provided with the
 *   distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 **********************************************************************/

#include "krb5_locl.h"

#ifndef _WIN32
#error  config_reg.c is only for Windows
#endif

#define REGPATH "SOFTWARE\\Kerberos"

/**
 * Parse a registry value as a string
 *
 * @see _krb5_parse_reg_value_as_multi_string()
 */
char *
_krb5_parse_reg_value_as_string(krb5_context context,
                                HKEY key, const char * valuename,
                                DWORD type, DWORD cb_data)
{
    return _krb5_parse_reg_value_as_multi_string(context, key, valuename,
                                                 type, cb_data, " ");
}

/**
 * Parse a registry value as a multi string
 *
 * The following registry value types are handled:
 *
 * - REG_DWORD: The decimal string representation is used as the
 *   value.
 *
 * - REG_SZ: The string is used as-is.
 *
 * - REG_EXPAND_SZ: Environment variables in the string are expanded
 *   and the result is used as the value.
 *
 * - REG_MULTI_SZ: The list of strings is concatenated using the
 *   separator.  No quoting is performed.
 *
 * Any other value type is rejected.
 *
 * @param [in]valuename Name of the registry value to be queried
 * @param [in]type      Type of the value. REG_NONE if unknown
 * @param [in]cbdata    Size of value. 0 if unknown.
 * @param [in]separator Separator character for concatenating strings.
 *
 * @a type and @a cbdata are only considered valid if both are
 * specified.
 *
 * @retval The registry value string, or NULL if there was an error.
 * If NULL is returned, an error message has been set using
 * krb5_set_error_message().
 */
char *
_krb5_parse_reg_value_as_multi_string(krb5_context context,
                                      HKEY key, const char * valuename,
                                      DWORD type, DWORD cb_data, char *separator)
{
    LONG                rcode = ERROR_MORE_DATA;

    BYTE                static_buffer[16384];
    BYTE                *pbuffer = &static_buffer[0];
    DWORD               cb_alloc = sizeof(static_buffer);
    char                *ret_string = NULL;

    /* If we know a type and cb_data from a previous call to
     * RegEnumValue(), we use it.  Otherwise we use the
     * static_buffer[] and query directly.  We do this to minimize the
     * number of queries. */

    if (type == REG_NONE || cb_data == 0) {

        pbuffer = &static_buffer[0];
        cb_alloc = cb_data = sizeof(static_buffer);
        rcode = RegQueryValueExA(key, valuename, NULL, &type, pbuffer, &cb_data);

        if (rcode == ERROR_SUCCESS &&

            ((type != REG_SZ &&
              type != REG_EXPAND_SZ) || cb_data + 1 <= sizeof(static_buffer)) &&

            (type != REG_MULTI_SZ || cb_data + 2 <= sizeof(static_buffer)))
            goto have_data;

        if (rcode != ERROR_MORE_DATA && rcode != ERROR_SUCCESS)
            return NULL;
    }

    /* Either we don't have the data or we aren't sure of the size
     * (due to potentially missing terminating NULs). */

    switch (type) {
    case REG_DWORD:
        if (cb_data != sizeof(DWORD)) {
            if (context)
                krb5_set_error_message(context, 0,
                                       "Unexpected size while reading registry value %s",
                                       valuename);
            return NULL;
        }
        break;

    case REG_SZ:
    case REG_EXPAND_SZ:

        if (rcode == ERROR_SUCCESS && cb_data > 0 && pbuffer[cb_data - 1] == '\0')
            goto have_data;

        cb_data += sizeof(char); /* Accout for potential missing NUL
                                  * terminator. */
        break;

    case REG_MULTI_SZ:

        if (rcode == ERROR_SUCCESS && cb_data > 0 && pbuffer[cb_data - 1] == '\0' &&
            (cb_data == 1 || pbuffer[cb_data - 2] == '\0'))
            goto have_data;

        cb_data += sizeof(char) * 2; /* Potential missing double NUL
                                      * terminator. */
        break;

    default:
        if (context)
            krb5_set_error_message(context, 0,
                                   "Unexpected type while reading registry value %s",
                                   valuename);
        return NULL;
    }

    if (cb_data <= sizeof(static_buffer))
        pbuffer = &static_buffer[0];
    else {
        pbuffer = malloc(cb_data);
        if (pbuffer == NULL)
            return NULL;
    }

    cb_alloc = cb_data;
    rcode = RegQueryValueExA(key, valuename, NULL, NULL, pbuffer, &cb_data);

    if (rcode != ERROR_SUCCESS) {

        /* This can potentially be from a race condition. I.e. some
         * other process or thread went and modified the registry
         * value between the time we queried its size and queried for
         * its value.  Ideally we would retry the query in a loop. */

        if (context)
            krb5_set_error_message(context, 0,
                                   "Unexpected error while reading registry value %s",
                                   valuename);
        goto done;
    }

    if (cb_data > cb_alloc || cb_data == 0) {
        if (context)
            krb5_set_error_message(context, 0,
                                   "Unexpected size while reading registry value %s",
                                   valuename);
        goto done;
    }

have_data:
    switch (type) {
    case REG_DWORD:
        asprintf(&ret_string, "%d", *((DWORD *) pbuffer));
        break;

    case REG_SZ:
    {
        char * str = (char *) pbuffer;

        if (str[cb_data - 1] != '\0') {
            if (cb_data < cb_alloc)
                str[cb_data] = '\0';
            else
                break;
        }

        if (pbuffer != static_buffer) {
            ret_string = (char *) pbuffer;
            pbuffer = NULL;
        } else {
            ret_string = strdup((char *) pbuffer);
        }
    }
    break;

    case REG_EXPAND_SZ:
    {
        char    *str = (char *) pbuffer;
        char    expsz[32768];   /* Size of output buffer for
                                 * ExpandEnvironmentStrings() is
                                 * limited to 32K. */

        if (str[cb_data - 1] != '\0') {
            if (cb_data < cb_alloc)
                str[cb_data] = '\0';
            else
                break;
        }

        if (ExpandEnvironmentStrings(str, expsz, sizeof(expsz)/sizeof(char)) != 0) {
            ret_string = strdup(expsz);
        } else {
            if (context)
                krb5_set_error_message(context, 0,
                                       "Overflow while expanding environment strings "
                                       "for registry value %s", valuename);
        }
    }
    break;

    case REG_MULTI_SZ:
    {
        char * str = (char *) pbuffer;
        char * iter;

        str[cb_alloc - 1] = '\0';
        str[cb_alloc - 2] = '\0';

        for (iter = str; *iter;) {
            size_t len = strlen(iter);

            iter += len;
            if (iter[1] != '\0')
                *iter++ = *separator;
            else
                break;
        }

        if (pbuffer != static_buffer) {
            ret_string = str;
            pbuffer = NULL;
        } else {
            ret_string = strdup(str);
        }
    }
    break;

    default:
        if (context)
            krb5_set_error_message(context, 0,
                                   "Unexpected type while reading registry value %s",
                                   valuename);
    }

done:
    if (pbuffer != static_buffer && pbuffer != NULL)
        free(pbuffer);

    return ret_string;
}

/**
 * Parse a registry value as a configuration value
 *
 * @see parse_reg_value_as_string()
 */
static krb5_error_code
parse_reg_value(krb5_context context,
                HKEY key, const char * valuename,
                DWORD type, DWORD cbdata, krb5_config_section ** parent)
{
    char                *reg_string = NULL;
    krb5_config_section *value;
    krb5_error_code     code = 0;

    reg_string = _krb5_parse_reg_value_as_string(context, key, valuename, type, cbdata);

    if (reg_string == NULL)
        return KRB5_CONFIG_BADFORMAT;

    value = _krb5_config_get_entry(parent, valuename, krb5_config_string);
    if (value == NULL) {
        code = ENOMEM;
        goto done;
    }

    if (value->u.string != NULL)
        free(value->u.string);

    value->u.string = reg_string;
    reg_string = NULL;

done:
    if (reg_string != NULL)
        free(reg_string);

    return code;
}

static krb5_error_code
parse_reg_values(krb5_context context,
                 HKEY key,
                 krb5_config_section ** parent)
{
    DWORD index;
    LONG  rcode;

    for (index = 0; ; index ++) {
        char    name[16385];
        DWORD   cch = sizeof(name)/sizeof(name[0]);
        DWORD   type;
        DWORD   cbdata = 0;
        krb5_error_code code;

        rcode = RegEnumValue(key, index, name, &cch, NULL,
                             &type, NULL, &cbdata);
        if (rcode != ERROR_SUCCESS)
            break;

        if (cbdata == 0)
            continue;

        code = parse_reg_value(context, key, name, type, cbdata, parent);
        if (code != 0)
            return code;
    }

    return 0;
}

static krb5_error_code
parse_reg_subkeys(krb5_context context,
                  HKEY key,
                  krb5_config_section ** parent)
{
    DWORD index;
    LONG  rcode;

    for (index = 0; ; index ++) {
        HKEY    subkey = NULL;
        char    name[256];
        DWORD   cch = sizeof(name)/sizeof(name[0]);
        krb5_config_section     *section = NULL;
        krb5_error_code         code;

        rcode = RegEnumKeyEx(key, index, name, &cch, NULL, NULL, NULL, NULL);
        if (rcode != ERROR_SUCCESS)
            break;

        rcode = RegOpenKeyEx(key, name, 0, KEY_READ, &subkey);
        if (rcode != ERROR_SUCCESS)
            continue;

        section = _krb5_config_get_entry(parent, name, krb5_config_list);
        if (section == NULL) {
            RegCloseKey(subkey);
            return ENOMEM;
        }

        code = parse_reg_values(context, subkey, &section->u.list);
        if (code) {
            RegCloseKey(subkey);
            return code;
        }

        code = parse_reg_subkeys(context, subkey, &section->u.list);
        if (code) {
            RegCloseKey(subkey);
            return code;
        }

        RegCloseKey(subkey);
    }

    return 0;
}

static krb5_error_code
parse_reg_root(krb5_context context,
               HKEY key,
               krb5_config_section ** parent)
{
    krb5_config_section *libdefaults = NULL;
    krb5_error_code     code = 0;

    libdefaults = _krb5_config_get_entry(parent, "libdefaults", krb5_config_list);
    if (libdefaults == NULL) {
        krb5_set_error_message(context, ENOMEM, "Out of memory while parsing configuration");
        return ENOMEM;
    }

    code = parse_reg_values(context, key, &libdefaults->u.list);
    if (code)
        return code;

    return parse_reg_subkeys(context, key, parent);
}

/**
 * Load configuration from registry
 *
 * The registry keys 'HKCU\Software\Heimdal' and
 * 'HKLM\Software\Heimdal' are treated as krb5.conf files.  Each
 * registry key corresponds to a configuration section (or bound list)
 * and each value in a registry key is treated as a bound value.  The
 * set of values that are directly under the Heimdal key are treated
 * as if they were defined in the [libdefaults] section.
 *
 * @see parse_reg_value() for details about how each type of value is handled.
 */
krb5_error_code
_krb5_load_config_from_registry(krb5_context context,
                                krb5_config_section ** res)
{
    HKEY        key = NULL;
    LONG        rcode;
    krb5_error_code code = 0;

    rcode = RegOpenKeyEx(HKEY_LOCAL_MACHINE, REGPATH, 0, KEY_READ, &key);
    if (rcode == ERROR_SUCCESS) {
        code = parse_reg_root(context, key, res);
        RegCloseKey(key);
        key = NULL;

        if (code)
            return code;
    }

    rcode = RegOpenKeyEx(HKEY_CURRENT_USER, REGPATH, 0, KEY_READ, &key);
    if (rcode == ERROR_SUCCESS) {
        code = parse_reg_root(context, key, res);
        RegCloseKey(key);
        key = NULL;

        if (code)
            return code;
    }

    return 0;
}
