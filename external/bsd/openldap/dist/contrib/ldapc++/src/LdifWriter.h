/*	$NetBSD: LdifWriter.h,v 1.1.1.2 2010/03/08 02:14:14 lukem Exp $	*/

// OpenLDAP: pkg/ldap/contrib/ldapc++/src/LdifWriter.h,v 1.2.2.1 2008/04/14 22:58:58 quanah Exp
/*
 * Copyright 2008, OpenLDAP Foundation, All Rights Reserved.
 * COPYING RESTRICTIONS APPLY, see COPYRIGHT file
 */

#ifndef LDIF_WRITER_H
#define LDIF_WRITER_H

#include <LDAPEntry.h>
#include <iosfwd>
#include <list>

class LdifWriter
{
    public:
        LdifWriter( std::ostream& output, int version = 0 );
        void writeRecord(const LDAPEntry& le);
        void writeIncludeRecord(const std::string& target);

    private:
        void breakline( const std::string &line, std::ostream &out );

        std::ostream& m_ldifstream;
        int m_version;
        bool m_addSeparator;

};

#endif /* LDIF_WRITER_H */

