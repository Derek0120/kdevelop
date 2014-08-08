/*
 * <one line to give the library's name and an idea of what it does.>
 * Copyright 2014  Milian Wolff <mail@milianw.de>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License or (at your option) version 3 or any later version
 * accepted by the membership of KDE e.V. (or its successor approved
 * by the membership of KDE e.V.), which shall act as a proxy
 * defined in Section 14 of version 3 of the license.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#ifndef CLANGPARSINGENVIRONMENTFILE_H
#define CLANGPARSINGENVIRONMENTFILE_H

#include <language/duchain/parsingenvironment.h>

#include "duchainexport.h"

class ClangParsingEnvironment;
struct ClangParsingEnvironmentFileData;

class KDEVCLANGDUCHAIN_EXPORT ClangParsingEnvironmentFile : public KDevelop::ParsingEnvironmentFile
{
public:
    ClangParsingEnvironmentFile(const KDevelop::IndexedString& url, const ClangParsingEnvironment& environment);
    ClangParsingEnvironmentFile(ClangParsingEnvironmentFileData& data);
    ~ClangParsingEnvironmentFile();

    virtual bool needsUpdate(const KDevelop::ParsingEnvironment* environment = 0) const;
    virtual int type() const;

    void setEnvironment(const ClangParsingEnvironment& environment);

    enum {
        Identity = 142
    };

    DUCHAIN_DECLARE_DATA(ClangParsingEnvironmentFile)
};

#endif // CLANGPARSINGENVIRONMENTFILE_H
