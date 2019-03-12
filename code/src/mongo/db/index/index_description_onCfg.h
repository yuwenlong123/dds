// index_descriptor.cpp

/**
*    Copyright (C) 2013 10gen Inc.
*
*    This program is free software: you can redistribute it and/or  modify
*    it under the terms of the GNU Affero General Public License, version 3,
*    as published by the Free Software Foundation.
*
*    This program is distributed in the hope that it will be useful,
*    but WITHOUT ANY WARRANTY; without even the implied warranty of
*    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
*    GNU Affero General Public License for more details.
*
*    You should have received a copy of the GNU Affero General Public License
*    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*
*    As a special exception, the copyright holders give permission to link the
*    code of portions of this program with the OpenSSL library under certain
*    conditions as described in each individual source file and distribute
*    linked combinations including the program with the OpenSSL library. You
*    must comply with the GNU Affero General Public License in all respects for
*    all of the code used other than as permitted herein. If you modify file(s)
*    with this exception, you may extend this exception to your version of the
*    file(s), but you are not obligated to do so. If you do not wish to do so,
*    delete this exception statement from your version. If you delete this
*    exception statement from all source files in the program, then also delete
*    it in the license file.
*/

#pragma once

#include <set>
#include <string>

#include "mongo/db/catalog/collection.h"
#include "mongo/db/index/multikey_paths.h"
#include "mongo/db/jsobj.h"
#include "mongo/db/server_options.h"
#include "mongo/util/assert_util.h"
#include "mongo/util/stacktrace.h"

namespace mongo {


/**
 * A cache of information computed from the memory-mapped per-index data (OnDiskIndexData).
 * Contains accessors for the various immutable index parameters, and an accessor for the
 * mutable "head" pointer which is index-specific.
 *
 * All synchronization is the responsibility of the caller.
 */
class IndexDescriptionOnCfg {
public:
    enum class IndexVersion { kV0 = 0, kV1 = 1, kV2 = 2 };

    static constexpr StringData k2dIndexBitsFieldName1 = "bits"_sd;
    static constexpr StringData k2dIndexMinFieldName1 = "min"_sd;
    static constexpr StringData k2dIndexMaxFieldName1 = "max"_sd;
    static constexpr StringData k2dsphereCoarsestIndexedLevel1 = "coarsestIndexedLevel"_sd;
    static constexpr StringData k2dsphereFinestIndexedLevel1 = "finestIndexedLevel"_sd;
    static constexpr StringData k2dsphereVersionFieldName1 = "2dsphereIndexVersion"_sd;
    static constexpr StringData kBackgroundFieldName1 = "background"_sd;
    static constexpr StringData kCollationFieldName1 = "collation"_sd;
    static constexpr StringData kDefaultLanguageFieldName1 = "default_language"_sd;
    static constexpr StringData kDropDuplicatesFieldName1 = "dropDups"_sd;
    static constexpr StringData kExpireAfterSecondsFieldName1 = "expireAfterSeconds"_sd;
    static constexpr StringData kGeoHaystackBucketSize1 = "bucketSize"_sd;
    static constexpr StringData kIndexNameFieldName1 = "name"_sd;
    static constexpr StringData kIndexVersionFieldName1 = "v"_sd;
    static constexpr StringData kKeyPatternFieldName1 = "key"_sd;
    static constexpr StringData kLanguageOverrideFieldName1 = "language_override"_sd;
    static constexpr StringData kNamespaceFieldName1 = "ns"_sd;
    static constexpr StringData kPartialFilterExprFieldName1 = "partialFilterExpression"_sd;
    static constexpr StringData kSparseFieldName1 = "sparse"_sd;
    static constexpr StringData kStorageEngineFieldName1 = "storageEngine"_sd;
    static constexpr StringData kTextVersionFieldName1 = "textIndexVersion"_sd;
    static constexpr StringData kUniqueFieldName1 = "unique"_sd;
    static constexpr StringData kWeightsFieldName1 = "weights"_sd;
    static constexpr StringData kPrefix1 = "prefix"_sd;

    static BSONObj getKeyPattern(const BSONObj& infoObj) {
        return infoObj.getObjectField(kKeyPatternFieldName1).getOwned();
    }

    static bool hasUniqueModifier(
        const BSONObj& infoObj)  //  Index is unique if it's _id or has unique modifier
    {
        return infoObj[kUniqueFieldName1].trueValue();
    }

    /**
     * OnDiskIndexData is a pointer to the memory mapped per-index data.
     * infoObj is a copy of the index-describing BSONObj contained in the OnDiskIndexData.
     */
    IndexDescriptionOnCfg(BSONObj infoObj)
        : _infoObj(infoObj.getOwned()),
          _keyPattern(getKeyPattern(infoObj)),
          _numFields(_keyPattern.nFields()),
          _indexName(infoObj.getStringField(IndexDescriptionOnCfg::kIndexNameFieldName1)),
          _parentNS(infoObj.getStringField(IndexDescriptionOnCfg::kNamespaceFieldName1)),
          _isIdIndex(isIdIndexPattern(_keyPattern)),
          _sparse(infoObj[IndexDescriptionOnCfg::kSparseFieldName1].trueValue()),
          _unique(_isIdIndex || hasUniqueModifier(infoObj)),
          _partial(!infoObj[IndexDescriptionOnCfg::kPartialFilterExprFieldName1].eoo()),
          _prefix(infoObj[IndexDescriptionOnCfg::kPrefix1].numberLong()) {
        _indexNamespace = makeIndexNamespace(_parentNS, _indexName);
        _version = IndexVersion::kV0;
        BSONElement e = _infoObj[IndexDescriptionOnCfg::kIndexVersionFieldName1];
        if (e.isNumber()) {
            _version = static_cast<IndexVersion>(e.numberInt());
        }
    }

    /**
     * Returns true if the specified index version is supported, and returns false otherwise.
     */
    static bool isIndexVersionSupported(IndexVersion indexVersion);

    /**
     * Returns a set of the currently supported index versions.
     */
    static std::set<IndexVersion> getSupportedIndexVersions();

    /**
     * Returns Status::OK() if indexes of version 'indexVersion' are allowed to be created, and
     * returns ErrorCodes::CannotCreateIndex otherwise.
     */
    static Status isIndexVersionAllowedForCreation(
        IndexVersion indexVersion,
        const ServerGlobalParams::FeatureCompatibility& featureCompatibility,
        const BSONObj& indexSpec);

    /**
     * Returns the index version to use if it isn't specified in the index specification.
     */
    static IndexVersion getDefaultIndexVersion(
        ServerGlobalParams::FeatureCompatibility::Version featureCompatibilityVersion);

    //
    // Information about the key pattern.
    //

    /**
     * Return the user-provided index key pattern.
     * Example: {geo: "2dsphere", nonGeo: 1}
     * Example: {foo: 1, bar: -1}
     */
    const BSONObj& keyPattern() const {
        return _keyPattern;
    }

    /**
     * Test only command for testing behavior resulting from an incorrect key
     * pattern.
     */
    void setKeyPatternForTest(BSONObj newKeyPattern) {
        _keyPattern = newKeyPattern;
    }

    // How many fields do we index / are in the key pattern?
    int getNumFields() const {
        return _numFields;
    }

    //
    // Information about the index's namespace / collection.
    //

    // Return the name of the index.
    const std::string& indexName() const {
        return _indexName;
    }

    // Return the name of the indexed collection.
    const std::string& parentNS() const {
        return _parentNS;
    }

    // Return the name of this index's storage area (database.table.$index)
    const std::string& indexNamespace() const {
        return _indexNamespace;
    }

    //
    // Properties every index has
    //

    // Return what version of index this is.
    IndexVersion version() const {
        return _version;
    }

    // May each key only occur once?
    bool unique() const {
        return _unique;
    }

    // Is this index sparse?
    bool isSparse() const {
        return _sparse;
    }

    // Is this a partial index?
    bool isPartial() const {
        return _partial;
    }

    bool isIdIndex() const {
        return _isIdIndex;
    }

    //
    // Properties that are Index-specific.
    //

    // Allow access to arbitrary fields in the per-index info object.  Some indices stash
    // index-specific data there.
    BSONElement getInfoElement(const std::string& name) const {
        return _infoObj[name];
    }

    //
    // "Internals" of accessing the index, used by IndexAccessMethod(s).
    //

    // Return a (rather compact) std::string representation.
    std::string toString() const {
        return _infoObj.toString();
    }

    // Return the info object.
    const BSONObj& infoObj() const {
        return _infoObj;
    }

    bool areIndexOptionsEquivalent(const IndexDescriptionOnCfg* other) const;

    static bool isIdIndexPatternEqual(const BSONObj& p1, const BSONObj& p2) {
        BSONObjIterator i(p1);
        BSONObjIterator j(p2);
        BSONElement e1 = i.next();
        BSONElement e2 = j.next();

        if ((strcmp(e1.fieldName(), "_id") == 0) && (strcmp(e2.fieldName(), "_id") == 0)) {
            if (e1.numberInt() == e2.numberInt()) {
                if (i.next().eoo() && j.next().eoo())
                    return true;
            }
        }
        return false;
    }
    static bool isIdIndexPattern(const BSONObj& pattern) {
        BSONObjIterator i(pattern);
        BSONElement e = i.next();
        //_id index must have form exactly {_id : 1} or {_id : -1}.
        // Allows an index of form {_id : "hashed"} to exist but
        // do not consider it to be the primary _id index
        if (!(strcmp(e.fieldName(), "_id") == 0 && (e.numberInt() == 1 || e.numberInt() == -1)))
            return false;
        return i.next().eoo();
    }

    static std::string makeIndexNamespace(StringData ns, StringData name) {
        return ns.toString() + ".$" + name.toString();
    }

    int64_t getPrefix() const {
        return _prefix;
    }

private:
    // The BSONObj describing the index.  Accessed through the various members above.
    const BSONObj _infoObj;

    // --- cached data from _infoObj

    BSONObj _keyPattern;
    int64_t _numFields;  // How many fields are indexed?
    std::string _indexName;
    std::string _parentNS;
    std::string _indexNamespace;
    bool _isIdIndex;
    bool _sparse;
    bool _unique;
    bool _partial;
    IndexVersion _version;

    // prefix for the index, given by assign command from config server
    int64_t _prefix;
};

}  // namespace mongo