#include <iostream>
#include <vector>
#include <cassert>
#include <cmath>
#include <time.h>
#ifdef Darwin
#include <mach/mach_time.h>
#endif
#ifdef _WINDOWS
#include "windows.h"
#include "Winbase.h"
#include <limits>
#endif

#include <vapor/utils.h>
#include <vapor/vizutil.h>
#include <vapor/UnstructuredGridLayered.h>

using namespace std;
using namespace VAPoR;

UnstructuredGridLayered::UnstructuredGridLayered(
    const std::vector<size_t> &vertexDims,
    const std::vector<size_t> &faceDims,
    const std::vector<size_t> &edgeDims,
    const std::vector<size_t> &bs,
    const std::vector<float *> &blks,
    const int *vertexOnFace,
    const int *faceOnVertex,
    const int *faceOnFace,
    Location location, // node,face, edge
    size_t maxVertexPerFace,
    size_t maxFacePerVertex,
    const UnstructuredGridCoordless &xug,
    const UnstructuredGridCoordless &yug,
    const UnstructuredGridCoordless &zug,
    const KDTreeRG *kdtree) : UnstructuredGrid(vertexDims, faceDims, edgeDims, bs, blks, 3,
                                               vertexOnFace, faceOnVertex, faceOnFace, location,
                                               maxVertexPerFace, maxFacePerVertex),
                              _ug2d(
                                  vector<size_t>{vertexDims[0]},
                                  vector<size_t>{faceDims[0]},
                                  edgeDims.size() ? vector<size_t>{edgeDims[0]} : vector<size_t>(),
                                  vector<size_t>{bs[0]},
                                  vector<float *>(), vertexOnFace, faceOnVertex, faceOnFace, location,
                                  maxVertexPerFace, maxFacePerVertex, xug, yug,
                                  UnstructuredGridCoordless(), kdtree),
                              _zug(zug) {

    assert(xug.GetDimensions().size() == 1);
    assert(yug.GetDimensions().size() == 1);
    assert(zug.GetDimensions().size() == 2);

    assert(location == NODE);
}

size_t UnstructuredGridLayered::GetGeometryDim() const {
    return (3);
}

void UnstructuredGridLayered::GetUserExtents(
    vector<double> &minu, vector<double> &maxu) const {
    minu.clear();
    maxu.clear();

    // Get horizontal extents from base class
    //
    _ug2d.GetUserExtents(minu, maxu);

    // Now add vertical extents
    //
    float range[2];
    _zug.GetRange(range);
    minu.push_back(range[0]);
    maxu.push_back(range[1]);
}

void UnstructuredGridLayered::GetBoundingBox(
    const vector<size_t> &min,
    const vector<size_t> &max,
    vector<double> &minu,
    vector<double> &maxu) const {

    vector<size_t> cMin = min;
    ClampIndex(cMin);

    vector<size_t> cMax = max;
    ClampIndex(cMax);

    vector<size_t> min2d = {cMin[0]};
    vector<size_t> max2d = {cMax[0]};

    _ug2d.GetBoundingBox(min2d, max2d, minu, maxu);

    float range[2];
    _zug.GetRange(min, max, range);
    minu.push_back(range[0]);
    maxu.push_back(range[1]);
}

void UnstructuredGridLayered::GetEnclosingRegion(
    const vector<double> &minu, const vector<double> &maxu,
    vector<size_t> &min, vector<size_t> &max) const {

    vector<double> cMinu = minu;
    ClampCoord(cMinu);

    vector<double> cMaxu = maxu;
    ClampCoord(cMaxu);

    assert(0 && "Not implemented");
}

void UnstructuredGridLayered::GetUserCoordinates(
    const std::vector<size_t> &indices,
    std::vector<double> &coords) const {

    vector<size_t> cIndices = indices;
    ClampIndex(cIndices);

    assert(cIndices.size() == 1);
    coords.clear();

    vector<size_t> indices2d = {cIndices[0]};
    _ug2d.GetUserCoordinates(indices2d, coords);

    coords.push_back(_zug.AccessIndex(cIndices));
}

void UnstructuredGridLayered::GetIndices(
    const std::vector<double> &coords,
    std::vector<size_t> &indices) const {

    indices.clear();

    // Clamp coordinates on periodic boundaries to grid extents
    //
    vector<double> cCoords = coords;
    ClampCoord(cCoords);

    vector<double> coords2D = {cCoords[0], cCoords[1]};
    _ug2d.GetIndices(coords2D, indices);

    int rc;
    size_t kFound = 0;
    vector<double> zcoords;

    size_t nz = GetDimensions()[1];
    for (int j = 0; j < nz; j++) {
        zcoords.push_back(_zug.AccessIJK(indices[0], j));
    }

    rc = Wasp::BinarySearchRange(zcoords, coords[2], kFound);

    if (rc < 0) {
        indices.push_back(0);
    } else if (rc > 0) {
        indices.push_back(GetDimensions()[1] - 1);
    } else {
        indices.push_back(kFound);
    }
}

bool UnstructuredGridLayered::_insideGrid(
    const std::vector<double> &coords,
    std::vector<size_t> &cindices,
    std::vector<size_t> &nodes2D,
    std::vector<double> &lambda,
    float zwgt[2]) const {
    assert(_location == NODE);

    cindices.clear();
    nodes2D.clear();
    lambda.clear();

    vector<double> cCoords = coords;
    ClampCoord(cCoords);

    // Find the 2D horizontal cell containing the X,Y coordinates
    //
    vector<double> coords2D = {cCoords[0], cCoords[1]};
    std::vector<std::vector<size_t>> nodes;

    bool status = _ug2d.GetIndicesCell(
        coords2D, cindices, nodes, lambda);
    if (!status)
        return (status);

    assert(lambda.size() == nodes.size());
    for (int i = 0; i < nodes.size(); i++) {
        nodes2D.push_back(nodes[i][0]);
    }

    // Find k index of cell containing z
    //
    vector<double> zcoords;

    size_t nz = GetDimensions()[1];
    for (int kk = 0; kk < nz; kk++) {
        float z = 0.0;
        for (int i = 0; i < lambda.size(); i++) {
            z += _zug.AccessIJK(nodes2D[i]) * lambda[i];
        }

        zcoords.push_back(z);
    }

    size_t k;
    int rc = Wasp::BinarySearchRange(zcoords, cCoords[2], k);

    if (rc != 0)
        return (false);

    assert(k >= 0 && k < nz);
    cindices.push_back(k);

    float z = cCoords[2];
    zwgt[0] = 1.0 - (z - zcoords[k]) / (zcoords[k + 1] - zcoords[k]);
    zwgt[1] = 1.0 - zwgt[0];

    return (true);
}

bool UnstructuredGridLayered::GetIndicesCell(
    const std::vector<double> &coords,
    std::vector<size_t> &indices) const {
    indices.clear();

    vector<size_t> nodes2D;
    vector<double> lambda;
    float zwgt[2];

    return (_insideGrid(coords, indices, nodes2D, lambda, zwgt));
}

bool UnstructuredGridLayered::InsideGrid(
    const std::vector<double> &coords) const {

    std::vector<size_t> indices;
    std::vector<size_t> nodes2D;
    vector<double> lambda;
    float zwgt[2];

    return (_insideGrid(coords, indices, nodes2D, lambda, zwgt));
}

float UnstructuredGridLayered::GetValueNearestNeighbor(
    const std::vector<double> &coords) const {
    std::vector<size_t> indices;
    std::vector<size_t> nodes2D;
    vector<double> lambda;
    float zwgt[2];

    bool inside = _insideGrid(coords, indices, nodes2D, lambda, zwgt);
    if (!inside)
        return (GetMissingValue());

    // Find nearest node in XY plane (the curvilinear part of grid)
    // The nearest node will have largest lambda resampling value.
    //
    float max_lambda = 0.0;
    int max_nodes2d_index = 0;
    for (int i = 0; i < lambda.size(); i++) {
        if (lambda[i] > max_lambda) {
            max_lambda = lambda[i];
            max_nodes2d_index = i;
        }
    }

    // Now find out which node is closest along vertical axis. We rely on
    // the Cell index returned in 'indices' being identical to the node ID
    // along the vertical axis ('cause its a layered grid)
    //
    int max_vert_id = indices[1];
    if (zwgt[1] > zwgt[0]) {
        max_vert_id += 1;
    }

    return (AccessIJK(nodes2D[max_nodes2d_index], max_vert_id));
}

float UnstructuredGridLayered::GetValueLinear(
    const std::vector<double> &coords) const {

    std::vector<size_t> indices;
    std::vector<size_t> nodes2D;
    vector<double> lambda;
    float zwgt[2];

    bool inside = _insideGrid(coords, indices, nodes2D, lambda, zwgt);
    if (!inside)
        return (GetMissingValue());

    // Interpolate value inside bottom face
    //
    float z0 = 0.0;
    size_t k0 = indices[1];
    for (int i = 0; i < lambda.size(); i++) {
        z0 += AccessIJK(nodes2D[i], k0) * lambda[i];
    }

    // Interpolate value inside top face
    //
    float z1 = 0.0;
    float k1 = indices[1] + 1;
    for (int i = 0; i < lambda.size(); i++) {
        z1 += AccessIJK(nodes2D[i], k1) * lambda[i];
    }

    return (z0 * zwgt[0] + z1 * zwgt[1]);
}

/////////////////////////////////////////////////////////////////////////////
//
// Iterators
//
/////////////////////////////////////////////////////////////////////////////

UnstructuredGridLayered::ConstCoordItrULayered::ConstCoordItrULayered(
    const UnstructuredGridLayered *ug, bool begin) : ConstCoordItrAbstract() {
    _ug = ug;
    _coords = vector<double>(3, 0.0);
    _nElements2D = ug->GetDimensions()[0];
    if (begin) {
        _itr2D = ug->_ug2d.ConstCoordBegin();
        _zCoordItr = ug->_zug.cbegin();
        _index2D = 0;
    } else {
        _itr2D = ug->_ug2d.ConstCoordEnd();
        _zCoordItr = ug->_zug.cend();
        _index2D = _nElements2D - 1;
    }
}

UnstructuredGridLayered::ConstCoordItrULayered::ConstCoordItrULayered(
    const ConstCoordItrULayered &rhs) : ConstCoordItrAbstract() {
    _ug = rhs._ug;
    _itr2D = rhs._itr2D;
    _zCoordItr = rhs._zCoordItr;
    _coords = rhs._coords;
    _nElements2D = rhs._nElements2D;
    _index2D = rhs._index2D;
}

UnstructuredGridLayered::ConstCoordItrULayered::ConstCoordItrULayered() : ConstCoordItrAbstract() {
}

void UnstructuredGridLayered::ConstCoordItrULayered::next() {

    ++_index2D;

    ++_itr2D;

    // Check for overflow
    //
    if (_index2D == _nElements2D) {
        _itr2D = _ug->_ug2d.ConstCoordBegin();
        _index2D = 0;
    }

    _coords[0] = (*_itr2D)[0];
    _coords[1] = (*_itr2D)[1];

    ++_zCoordItr;
    _coords[2] = *_zCoordItr;
}

void UnstructuredGridLayered::ConstCoordItrULayered::next(const long &offset) {
    assert(offset >= 0);

    long offset2D = offset % _nElements2D;

    if (offset2D + _index2D < _nElements2D) {
        _itr2D += offset;
        _index2D += offset;
    } else {
        size_t o = (offset2D + _index2D) % _nElements2D;
        _itr2D = _ug->_ug2d.ConstCoordBegin() + o;
        _index2D = o;
    }

    _coords[0] = (*_itr2D)[0];
    _coords[1] = (*_itr2D)[1];

    _zCoordItr += offset;
    _coords[2] = *_zCoordItr;
}

namespace VAPoR {
std::ostream &operator<<(std::ostream &o, const UnstructuredGridLayered &ug) {
    o << "UnstructuredGridLayered " << endl;
    o << (const Grid &)ug;

    return o;
}
}; // namespace VAPoR
