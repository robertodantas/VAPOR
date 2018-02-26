#include <stdio.h>
#include <iostream>
#include <cassert>
#include <cmath>
#include <cfloat>
#include <vapor/utils.h>
#include <vapor/CurvilinearGrid.h>
#include <vapor/KDTreeRG.h>
#include <vapor/vizutil.h>

using namespace std;
using namespace VAPoR;

void CurvilinearGrid::_curvilinearGrid(
    const RegularGrid &xrg,
    const RegularGrid &yrg,
    const vector<double> &zcoords,
    const KDTreeRG *kdtree) {
    _zcoords.clear();
    _minu.clear();
    _maxu.clear();
    _kdtree = kdtree;
    _xrg = xrg;
    _yrg = yrg;
    _zcoords = zcoords;
}

CurvilinearGrid::CurvilinearGrid(
    const vector<size_t> &dims,
    const vector<size_t> &bs,
    const vector<float *> &blks,
    const RegularGrid &xrg,
    const RegularGrid &yrg,
    const vector<double> &zcoords,
    const KDTreeRG *kdtree) : StructuredGrid(dims, bs, blks) {

    assert(bs.size() == dims.size());
    assert(bs.size() >= 1 && bs.size() <= 3);

    // Only support 2D X & Y coordinates currently. I.e. only support
    // "layered" curvilinear grids
    //
    assert(xrg.GetDimensions().size() == 2);
    assert(yrg.GetDimensions().size() == 2);
    assert(kdtree->GetDimensions().size() == 2);

    _curvilinearGrid(xrg, yrg, zcoords, kdtree);
}

size_t CurvilinearGrid::GetGeometryDim() const {
    return (_zcoords.size() == 0 ? 2 : 3);
}

void CurvilinearGrid::GetBoundingBox(
    const std::vector<size_t> &min, const std::vector<size_t> &max,
    std::vector<double> &minu, std::vector<double> &maxu) const {

    vector<size_t> cMin = min;
    ClampIndex(cMin);

    vector<size_t> cMax = max;
    ClampIndex(cMax);

    for (int i = 0; i < cMin.size(); i++) {
        assert(cMin[i] <= cMax[i]);
    }

    minu.clear();
    maxu.clear();

    for (int i = 0; i < cMin.size(); i++) {
        minu.push_back(0.0);
        maxu.push_back(0.0);
    }

    // Get the horiztonal (X & Y) extents by visiting every point
    // on a single plane (horizontal coordinates are constant over Z).
    //
    vector<size_t> min2d = {cMin[0], cMin[1]};
    vector<size_t> max2d = {cMax[0], cMax[1]};
    float xrange[2], yrange[2];
    _xrg.GetRange(min2d, max2d, xrange);
    _yrg.GetRange(min2d, max2d, yrange);

    minu[0] = xrange[0];
    minu[1] = yrange[0];
    maxu[0] = xrange[1];
    maxu[1] = yrange[1];

    // We're done if 2D grid
    //
    if (GetGeometryDim() == 2)
        return;

    minu[2] = _zcoords[cMin[2]];
    maxu[2] = _zcoords[cMax[2]];
}

void CurvilinearGrid::GetEnclosingRegion(
    const std::vector<double> &minu, const std::vector<double> &maxu,
    std::vector<size_t> &min, std::vector<size_t> &max) const {

    // Clamp coordinates on periodic boundaries to grid extents
    //
    vector<double> cMinu = minu;
    ClampCoord(cMinu);

    vector<double> cMaxu = maxu;
    ClampCoord(cMaxu);

    assert(cMinu.size() == cMaxu.size());

    // Initialize voxels coords to full grid
    //
    vector<size_t> dims = GetDimensions();
    for (int i = 0; i < dims.size(); i++) {
        min[i] = 0;
        max[i] = dims[i] - 1;
    }

    // Now shrink min and max to the smallest box containing the region
    //

    // Find min and max Y voxel
    //
    float xmin = cMinu[0];
    float ymin = cMinu[1];
    int jmin = min[1];
    bool outside = true;
    for (int j = 0; j < dims[1] && outside; j++) {
        for (int i = 0; i < dims[0] && outside; i++) {
            float x = _xrg.AccessIJK(i, j, 0);
            float y = _yrg.AccessIJK(i, j, 0);

            if (!(x > xmin && y > ymin))
                outside = false;
        }
        if (outside)
            jmin = j;
    }
    min[1] = jmin;

    float xmax = cMaxu[0];
    float ymax = cMaxu[1];
    int jmax = max[1];
    outside = true;
    for (int j = dims[1] - 1; j >= 0 && outside; j--) {
        for (int i = 0; i < dims[0] && outside; i++) {
            float x = _xrg.AccessIJK(i, j, 0);
            float y = _yrg.AccessIJK(i, j, 0);

            if (!(x > xmin && y > ymin))
                outside = false;
        }
        if (outside)
            jmax = j;
    }
    max[1] = jmax;

    // Find min and max X voxel
    //
    int imin = min[0];
    outside = true;
    for (int i = 0; i < dims[0] && outside; i++) {
        for (int j = min[1]; j <= max[1] && outside; j++) {
            float x = _xrg.AccessIJK(i, j, 0);
            float y = _yrg.AccessIJK(i, j, 0);

            if (!(x < xmax && y < ymax))
                outside = false;
        }
        if (outside)
            imin = i;
    }
    min[0] = imin;

    int imax = max[0];
    outside = true;
    for (int i = dims[0] - 1; i >= 0 && outside; i--) {
        for (int j = min[1]; j <= max[1] && outside; j++) {
            float x = _xrg.AccessIJK(i, j, 0);
            float y = _yrg.AccessIJK(i, j, 0);

            if (!(x < xmax && y < ymax))
                outside = false;
        }
        if (outside)
            imax = i;
    }
    max[0] = imax;

    if (dims.size() < 3)
        return; // 2D => we're done.

    // Finally, get Z
    //
    float zmin = cMinu[2];
    int kmin = min[2];
    outside = true;
    for (int k = 0; k < _zcoords.size() && outside; k++) {
        if (_zcoords[k] < zmin)
            outside = false;
        if (outside)
            kmin = k;
    }
    min[2] = kmin;

    float zmax = cMaxu[2];
    int kmax = max[2];
    outside = true;
    for (int k = _zcoords.size() - 1; k >= min[2] && outside; k--) {
        if (_zcoords[k] > zmax)
            outside = false;
        if (outside)
            kmax = k;
    }
    max[2] = kmax;
}

void CurvilinearGrid::GetUserCoordinates(
    const std::vector<size_t> &indices,
    std::vector<double> &coords) const {
    vector<size_t> cIndices = indices;
    ClampIndex(cIndices);

    coords.clear();

    vector<size_t> dims = StructuredGrid::GetDimensions();

    vector<size_t> cIndices2D = {cIndices[0], cIndices[1]};

    coords.push_back(_xrg.AccessIndex(cIndices2D));
    coords.push_back(_yrg.AccessIndex(cIndices2D));
    if (GetGeometryDim() > 2) {
        coords.push_back(_zcoords[cIndices[2]]);
    }
}

void CurvilinearGrid::GetIndices(
    const std::vector<double> &coords,
    std::vector<size_t> &indices) const {
    indices.clear();

    // Clamp coordinates on periodic boundaries to grid extents
    //
    vector<double> cCoords = coords;
    ClampCoord(cCoords);

    // First get horizontal coordinates, which are on curvilinear grid
    //
    vector<double> coords2D = {cCoords[0], cCoords[1]};
    _kdtree->Nearest(coords2D, indices);

    if (cCoords.size() == 2)
        return;

    size_t k;
    int rc = _binarySearchRange(_zcoords, cCoords[2], k);
    if (rc < 0) {
        indices.push_back(0);
    } else if (rc > 0) {
        indices.push_back(k);
    } else {
        indices.push_back(GetDimensions()[2] - 1);
    }
}

bool CurvilinearGrid::GetIndicesCell(
    const std::vector<double> &coords,
    std::vector<size_t> &indices) const {

    // Clamp coordinates on periodic boundaries to grid extents
    //
    vector<double> cCoords = coords;
    ClampCoord(cCoords);

    double x = cCoords[0];
    double y = cCoords[1];
    double z = GetGeometryDim() == 3 ? cCoords[2] : 0.0;

    double lambda[4], zwgt[2];
    size_t i, j, k;
    bool inside = _insideGrid(x, y, z, i, j, k, lambda, zwgt);

    if (!inside)
        return (false);

    indices.push_back(i);
    indices.push_back(j);

    if (GetGeometryDim() == 2)
        return (true);

    indices.push_back(k);

    return (true);
}

bool CurvilinearGrid::InsideGrid(const std::vector<double> &coords) const {

    // Clamp coordinates on periodic boundaries to reside within the
    // grid extents
    //
    vector<double> cCoords = coords;
    ClampCoord(cCoords);

    // Do a quick check to see if the point is completely outside of
    // the grid bounds.
    //
    for (int i = 0; i < cCoords.size(); i++) {
        if (cCoords[i] < _minu[i] || cCoords[i] > _maxu[i])
            return (false);
    }

    double lambda[4], zwgt[2];
    size_t i, j, k; // not used
    double x = cCoords[0];
    double y = cCoords[1];
    double z = GetGeometryDim() == 3 ? cCoords[2] : 0.0;

    bool inside = _insideGrid(x, y, z, i, j, k, lambda, zwgt);

    return (inside);
}

CurvilinearGrid::ConstCoordItrCG::ConstCoordItrCG(
    const CurvilinearGrid *cg, bool begin) : ConstCoordItrAbstract() {
    _cg = cg;
    vector<size_t> dims = _cg->GetDimensions();
    _index = vector<size_t>(dims.size(), 0);
    if (begin) {
        _xCoordItr = _cg->_xrg.cbegin();
        _yCoordItr = _cg->_yrg.cbegin();
    } else {
        _xCoordItr = _cg->_xrg.cend();
        _yCoordItr = _cg->_yrg.cend();
        _index[dims.size() - 1] = dims[dims.size() - 1];
    }
    _coords.push_back(*_xCoordItr);
    _coords.push_back(*_yCoordItr);
    if (dims.size() == 3) {
        _coords.push_back(_cg->_zcoords[0]);
    }
}

CurvilinearGrid::ConstCoordItrCG::ConstCoordItrCG(
    const ConstCoordItrCG &rhs) : ConstCoordItrAbstract() {
    _cg = rhs._cg;
    _index = rhs._index;
    _coords = rhs._coords;
    _xCoordItr = rhs._xCoordItr;
    _yCoordItr = rhs._yCoordItr;
}

CurvilinearGrid::ConstCoordItrCG::ConstCoordItrCG() : ConstCoordItrAbstract() {
    _cg = NULL;
    _index.clear();
    _coords.clear();
}

void CurvilinearGrid::ConstCoordItrCG::next() {

    const vector<size_t> &dims = _cg->GetDimensions();

    _index[0]++;
    ++_xCoordItr;
    ++_yCoordItr;

    if (_index[0] < dims[0]) {
        _coords[0] = *_xCoordItr;
        _coords[1] = *_yCoordItr;
        return;
    }

    _index[0] = 0;
    _index[1]++;

    if (_index[1] < dims[1]) {
        _coords[0] = *_xCoordItr;
        _coords[1] = *_yCoordItr;
        return;
    }

    if (dims.size() == 2)
        return;

    _index[1] = 0;
    _index[2]++;
    if (_index[2] < dims[2]) {
        _xCoordItr = _cg->_xrg.cbegin();
        _yCoordItr = _cg->_yrg.cbegin();

        _coords[0] = *_xCoordItr;
        _coords[1] = *_yCoordItr;
        _coords[2] = _cg->_zcoords[_index[2]];
        return;
    }
}

void CurvilinearGrid::ConstCoordItrCG::next(const long &offset) {

    if (!_index.size())
        return;

    const vector<size_t> &dims = _cg->GetDimensions();

    vector<size_t> maxIndex;
    ;
    for (int i = 0; i < dims.size(); i++)
        maxIndex.push_back(dims[i] - 1);

    long maxIndexL = Wasp::LinearizeCoords(maxIndex, dims);
    long newIndexL = Wasp::LinearizeCoords(_index, dims) + offset;
    if (newIndexL < 0) {
        newIndexL = 0;
    }
    if (newIndexL > maxIndexL) {
        _index = vector<size_t>(dims.size(), 0);
        _index[dims.size() - 1] = dims[dims.size() - 1];
        return;
    }

    size_t index2DL = _index[1] * dims[0] + _index[0];

    _index = Wasp::VectorizeCoords(newIndexL, dims);

    size_t offset2D = (long)(_index[1] * dims[0] + _index[0]) - (long)index2DL;

    _xCoordItr += offset2D;
    _yCoordItr += offset2D;

    _coords[0] = *_xCoordItr;
    _coords[1] = *_yCoordItr;

    if (dims.size() == 2)
        return;

    _coords[2] = _cg->_zcoords[_index[2]];
}

float CurvilinearGrid::GetValueNearestNeighbor(
    const std::vector<double> &coords) const {

    // Clamp coordinates on periodic boundaries to grid extents
    //
    vector<double> cCoords = coords;
    ClampCoord(cCoords);

    double lambda[4], zwgt[2];
    size_t i, j, k;
    double x = cCoords[0];
    double y = cCoords[1];
    double z = GetGeometryDim() == 3 ? cCoords[2] : 0.0;
    bool inside = _insideGrid(x, y, z, i, j, k, lambda, zwgt);

    if (!inside)
        return (GetMissingValue());

    return (AccessIJK(i, j, k));
}

namespace {

float interpolateQuad(
    const float values[4], const double lambda[4], float mv) {

    double lambda0[] = {lambda[0], lambda[1], lambda[2], lambda[3]};

    // Look for missing values. If found, zero out weight
    //
    float wTotal = 0.0;
    int nMissing = 0;
    for (int i = 0; i < 4; i++) {
        if (values[i] == mv) {
            lambda0[i] = 0.0;
            nMissing++;
        } else {
            wTotal += lambda0[i];
        }
    }

    // Re-normalize weights if we have missing values
    //
    if (nMissing) {
        wTotal = 1.0 / wTotal;
        for (int i = 0; i < 4; i++) {
            lambda0[i] *= wTotal;
        }
    }

    float v = 0.0;
    if (nMissing == 4) {
        v = mv;
    } else {
        for (int i = 0; i < 4; i++) {
            v += values[i] * lambda0[i];
        }
    }

    return (v);
}
}; // namespace

float CurvilinearGrid::GetValueLinear(
    const std::vector<double> &coords) const {

    // Clamp coordinates on periodic boundaries to grid extents
    //
    vector<double> cCoords = coords;
    ClampCoord(cCoords);

    // Get Wachspress coordinates for horizontal weights, and
    // simple linear interpolation weights for vertical axis. _insideGrid
    // handlese case where grid is 2D. I.e. if 2d then zwgt[0] == 1 &&
    // zwgt[1] = 0.0
    //
    double lambda[4], zwgt[2];
    size_t i, j, k;
    double x = cCoords[0];
    double y = cCoords[1];
    double z = GetGeometryDim() == 3 ? cCoords[2] : 0.0;
    bool inside = _insideGrid(x, y, z, i, j, k, lambda, zwgt);

    float mv = GetMissingValue();

    if (!inside)
        return (mv);

    // Use Wachspress coordinates as weights to do linear interpolation
    // along XY plane
    //
    vector<size_t> dims = GetDimensions();
    assert(i < dims[0] - 1);
    assert(j < dims[1] - 1);
    if (dims.size() > 2)
        assert(k < dims[2] - 1);

    float v0s[] = {
        AccessIJK(i, j, k),
        AccessIJK(i + 1, j, k),
        AccessIJK(i + 1, j + 1, k),
        AccessIJK(i, j + 1, k)};

    float v0 = interpolateQuad(v0s, lambda, mv);

    if (GetGeometryDim() == 2)
        return (v0);

    if (v0 == mv)
        zwgt[0] = 0.0;

    float v1s[] = {
        AccessIJK(i, j, k + 1),
        AccessIJK(i + 1, j, k + 1),
        AccessIJK(i + 1, j + 1, k + 1),
        AccessIJK(i, j + 1, k + 1)};

    float v1 = interpolateQuad(v1s, lambda, mv);

    if (v1 == mv)
        zwgt[1] = 0.0;

    // Linearly interpolate along Z axis
    //
    if (zwgt[0] == 0.0)
        return (v1);
    else if (zwgt[1] == 0.0)
        return (v0);
    else
        return (v0 * zwgt[0] + v1 * zwgt[1]);
}

void CurvilinearGrid::_GetUserExtents(
    vector<double> &minext, vector<double> &maxext) const {

    vector<size_t> dims = StructuredGrid::GetDimensions();

    vector<size_t> min, max;
    for (int i = 0; i < dims.size(); i++) {
        min.push_back(0);
        max.push_back(dims[i] - 1);
    }

    CurvilinearGrid::GetBoundingBox(min, max, minext, maxext);
}

// Perform a binary search in a sorted 1D vector of values for the
// entry that it closest to 'x'. Return the offset 'i' of 'x' in
// 'sorted'
//
int CurvilinearGrid::_binarySearchRange(
    const vector<double> &sorted,
    double x,
    size_t &i) const {
    i = 0;

    // See if above or below the array
    //
    if (x < sorted[0])
        return (-1);
    if (x > sorted[sorted.size() - 1])
        return (1);

    // Binary search for starting index of cell containing x
    //
    size_t i0 = 0;
    size_t i1 = sorted.size() - 1;
    double x0 = sorted[i0];
    double x1 = sorted[i1];
    while (i1 - i0 > 1) {

        x1 = sorted[(i0 + i1) >> 1];
        if (x1 == x) { // pathological case
            i0 = (i0 + i1) >> 1;
            break;
        }

        // if the signs of differences change then the coordinate
        // is between x0 and x1
        //
        if ((x - x0) * (x - x1) <= 0.0) {
            i1 = (i0 + i1) >> 1;
        } else {
            i0 = (i0 + i1) >> 1;
            x0 = x1;
        }
    }
    i = i0;
    return (0);
}

// Search for a point inside the grid. If the point is inside return true,
// and provide the Wachspress weights/coordinates for the point within
// the XY quadrilateral cell containing the point in XY, and the linear
// interpolation weights/coordinates along Z. If the grid is 2D then
// zwgt[0] == 1.0, and zwgt[1] == 0.0. If the point is outside of the
// grid the values of 'lambda', and 'zwgt' are not defined
//
bool CurvilinearGrid::_insideGrid(
    double x, double y, double z,
    size_t &i, size_t &j, size_t &k,
    double lambda[4], double zwgt[2]) const {
    for (int l = 0; l < 4; l++)
        lambda[l] = 0.0;
    for (int l = 0; l < 2; l++)
        zwgt[l] = 0.0;
    i = j = k = 0;

    vector<float> coordu;
    coordu.push_back(x);
    coordu.push_back(y);

    // Find the indeces for the nearest grid point in the horizontal plane
    //
    vector<size_t> indices;
    _kdtree->Nearest(coordu, indices);
    assert(indices.size() == 2);

    vector<size_t> dims = StructuredGrid::GetDimensions();

    // Now visit each quadrilateral that shares a vertex with the returned
    // grid indeces. Use Wachspress coordinates to determine if point is
    // inside a quad.
    //
    // First handle boundary cases
    //
    size_t i0 = (indices[0] > 0) ? indices[0] - 1 : 0;
    size_t i1 = (indices[0] < dims[0] - 1) ? indices[0] : dims[0] - 2;
    size_t j0 = (indices[1] > 0) ? indices[1] - 1 : 0;
    size_t j1 = (indices[1] < dims[1] - 1) ? indices[1] : dims[1] - 2;

    // Now walk the surrounding quads. If found (inside==true),
    // i and J are set to horizontal indices.
    //
    bool inside = false;
    double pt[] = {x, y};
    double verts[8];
    for (int jj = j0; jj <= j1 && !inside; jj++) {
        for (int ii = i0; ii <= i1 && !inside; ii++) {
            verts[0] = _xrg.AccessIJK(ii, jj, 0);
            verts[1] = _yrg.AccessIJK(ii, jj, 0);
            verts[2] = _xrg.AccessIJK(ii + 1, jj, 0);
            verts[3] = _yrg.AccessIJK(ii + 1, jj, 0);
            verts[4] = _xrg.AccessIJK(ii + 1, jj + 1, 0);
            verts[5] = _yrg.AccessIJK(ii + 1, jj + 1, 0);
            verts[6] = _xrg.AccessIJK(ii, jj + 1, 0);
            verts[7] = _yrg.AccessIJK(ii, jj + 1, 0);
            inside = VAPoR::WachspressCoords2D(verts, pt, 4, lambda);
            if (inside) {
                i = ii;
                j = jj;
            }
        }
    }

    if (!inside) {
        return (false);
    }

    if (GetGeometryDim() == 2) {
        zwgt[0] = 1.0;
        zwgt[1] = 0.0;
        return (true);
    }

    // Now verify that Z coordinate of point is in grid, and find
    // its interpolation weights if so.
    //
    int rc = _binarySearchRange(_zcoords, z, k);

    if (rc != 0)
        return (false);

    zwgt[0] = 1.0 - (z - _zcoords[k]) / (_zcoords[k + 1] - _zcoords[k]);
    zwgt[1] = 1.0 - zwgt[0];

    return (true);
}

void CurvilinearGrid::_getMinCellExtents(
    vector<double> &minCellExtents) const {

    minCellExtents.clear();

    vector<size_t> dims = StructuredGrid::GetDimensions();

    // Find minimum cell extents along X
    //
    float minx = _xrg.AccessIJK(1, 0, 0) - _xrg.AccessIJK(0, 0, 0);
    for (int j = 0; j < dims[1]; j++) {
        float x0 = _xrg.AccessIJK(0, j, 0);

        for (int i = 1; i < dims[0]; i++) {
            float x1 = _xrg.AccessIJK(i, j, 0);

            if ((x1 - x0) < minx)
                minx = x1 - x0;

            x1 = x0;
        }
    }
    minCellExtents.push_back(minx);

    // Find minimum cell extents along Y
    //
    float miny = _yrg.AccessIJK(0, 1, 0) - _yrg.AccessIJK(0, 0, 0);
    for (int i = 0; i < dims[0]; i++) {
        float y0 = _yrg.AccessIJK(i, 0, 0);

        for (int j = 1; j < dims[1]; j++) {
            float y1 = _yrg.AccessIJK(i, j, 0);

            if ((y1 - y0) < miny)
                miny = y1 - y0;

            y1 = y0;
        }
    }
    minCellExtents.push_back(miny);

    if (dims.size() < 3 || dims[2] < 2)
        return;

    // Find minimum cell extents along Z
    //
    float minz = _zcoords[1] - _zcoords[0];
    float z0 = _zcoords[0];
    for (int k = 1; k < dims[2]; k++) {
        float z1 = _zcoords[k];

        if ((z1 - z0) < minz)
            minz = z1 - z0;

        z1 = z0;
    }
    minCellExtents.push_back(minz);
}
