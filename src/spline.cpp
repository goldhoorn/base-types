#include "spline.h"
#include "sisl/sisl.h"

#include <stdexcept>
#include <vector>

using namespace std;
using namespace base::geometry;
using namespace Eigen;

static double angleLimit(double angle)
{
    if(angle > M_PI)
	return angle - 2*M_PI;
    else if (angle < -M_PI)
	return angle + 2*M_PI;
    else
     	return angle;
}

SplineBase::SplineBase (int dim, double _geometric_resolution, int _curve_order)
    : dimension(dim), curve(0), geometric_resolution(_geometric_resolution)
    , curve_order(_curve_order)
    , start_param(0), end_param(0)
    , has_curve_length(false), curve_length(-1)
    , has_curvature_max(false), curvature_max(-1)
{
}

SplineBase::SplineBase(double geometric_resolution, SISLCurve* curve)
    : dimension(curve->idim), curve(curve), geometric_resolution(geometric_resolution)
    , curve_order(curve->ik)
    , start_param(0), end_param(0)
    , has_curve_length(false), curve_length(-1)
    , has_curvature_max(false), curvature_max(-1)
{
    int status;

    s1363(curve, &start_param, &end_param, &status);
    if (status != 0)
        throw std::runtime_error("cannot get the curve start & end parameters");
}

SplineBase::~SplineBase ()
{
    // Frees the memory of the curve 
    if (curve)
        freeCurve(curve);
}

SplineBase::SplineBase(SplineBase const& source)
    : singleton(source.singleton), dimension(source.dimension)
    , curve(source.curve ? copyCurve(source.curve) : 0)
    , geometric_resolution(source.geometric_resolution)
    , curve_order(source.curve_order)
    , start_param(source.start_param), end_param(source.end_param)
    , has_curve_length(source.has_curve_length), curve_length(source.curve_length)
    , has_curvature_max(source.has_curvature_max), curvature_max(source.curvature_max)
{
}

SplineBase const& SplineBase::operator = (SplineBase const& source)
{
    if (&source == this)
        return *this;

    if (curve)
        freeCurve(curve);

    singleton            = source.singleton;
    dimension            = source.dimension;
    curve                = source.curve ? copyCurve(source.curve) : 0;
    geometric_resolution = source.geometric_resolution;
    curve_order          = source.curve_order;
    start_param          = source.start_param;
    end_param            = source.end_param;
    has_curve_length     = source.has_curve_length;
    curve_length         = source.curve_length;
    has_curvature_max    = source.has_curvature_max;
    curvature_max        = source.curvature_max;
    return *this;
}

int SplineBase::getPointCount() const
{
    if (curve)
        return curve->in;
    else
        return (singleton.empty() ? 0 : 1);
}

void SplineBase::getPoint(double* result, double _param) const
{ return getPointAndTangentHelper(result, _param, false); }

void SplineBase::getPointAndTangent(double* result, double _param) const
{ return getPointAndTangentHelper(result, _param, true); }

void SplineBase::getPointAndTangentHelper(double* result, double _param, bool with_tangent) const
{
    if (_param < start_param || _param > end_param) 
        throw std::out_of_range("_param is not in the [start_param, end_param] range");

    if (curve)
    {
        int leftknot; // Not needed
        int status;
        s1227(curve, (with_tangent ? 1 : 0), _param,
                &leftknot, result, &status); // Gets the point
        if (status != 0)
            throw std::runtime_error("SISL error while computing a curve point");
    }
    else
    {
        copy(singleton.begin(), singleton.end(), result);
        if (with_tangent)
        {
            int const dim = getDimension();
            for (int i = 0; i < dim; ++i)
                result[i + dim] = 0;
        }
    }
}

double SplineBase::getCurvature(double _param)
{
    // Limits the input paramter to the curve limit
    if (_param < start_param || _param > end_param) 
        throw std::out_of_range("_param is not in the [start_param, end_param] range");
    else if (!singleton.empty())
        throw std::runtime_error("getCurvature() called on a singleton");
    else if (!curve)
        throw std::runtime_error("getCurvature() called on an empty curve");

    double curvature; 
    int status;
    s2550(curve, &_param, 1, &curvature, &status); // Gets the point
    if (status != 0)
        throw std::runtime_error("SISL error while computing a curvature");

    return curvature;
}

double SplineBase::getVariationOfCurvature(double _param)  // Variation of Curvature
{
    if (_param < start_param || _param > end_param) 
        throw std::out_of_range("_param is not in the [start_param, end_param] range");
    else if (!singleton.empty())
        throw std::runtime_error("getVariationOfCurvature() called on a singleton");
    else if (!curve)
        throw std::runtime_error("getVariationOfCurvature() called on an empty curve");

    double VoC; 
    int status;
    s2556(curve, &_param, 1, &VoC, &status); // Gets the point
    if (status != 0)
        throw std::runtime_error("SISL error while computing a variation of curvature");

    return VoC;
}

double SplineBase::getCurveLength()
{
    if (has_curve_length)
        return curve_length;

    if (!curve)
        throw std::runtime_error("getCurveLength() called on an empty curve");

    int status;
    s1240(curve, geometric_resolution, &curve_length, &status);
    if (status != 0)
        throw std::runtime_error("cannot get the curve length");

    has_curve_length = true;
    return curve_length;
}

double SplineBase::getUnitParameter()
{
    if (end_param == start_param) return 0;
    return (end_param - start_param) / getCurveLength();
}

double SplineBase::getCurvatureMax()
{
    if (!singleton.empty())
        throw std::runtime_error("getCurvatureMax() called on a singleton");
    else if (!curve)
        throw std::runtime_error("getCurvatureMax() called on an empty curve");

    if (has_curvature_max)
        return curvature_max;

    double const delPara = getUnitParameter() * geometric_resolution;
    curvature_max = 0.0;

    for (double p = start_param; p <= end_param; p+= delPara)
    {
	double curvature = getCurvature(p);
	if (curvature > curvature_max)
	    curvature_max = curvature;
    }

    has_curvature_max = true;
    return curvature_max;
}

bool SplineBase::isNURBS() const
{
    if (!curve) return false;
    return curve->ikind == 2 || curve->ikind == 4;
}

void SplineBase::interpolate(std::vector<double> const& points, std::vector<double> const& parameters)
{
    clear();
    start_param = 0.0;
    has_curvature_max = false;
    has_curve_length  = false;

    int const point_count = points.size() / dimension;
    if (point_count == 0)
    {
        end_param = 0;
        singleton.clear();
        return;
    }
    else if (point_count == 1)
    {
        end_param = 0;
        has_curve_length = true;
        curve_length = 0;
        singleton = points;
        return;
    }

    singleton.clear();
    vector<int> point_types;
    point_types.resize(point_count, 1);

    // Generates curve
    double* point_param;  
    int nb_unique_param;

    int status;
    if (parameters.empty())
    {
        s1356(const_cast<double*>(&points[0]), point_types.size(), dimension, &point_types[0],
                0, 0, 1, curve_order, start_param, &end_param, &curve, 
                &point_param, &nb_unique_param, &status);
    }
    else
    {
        s1357(const_cast<double*>(&points[0]), point_types.size(), dimension, &point_types[0],
                const_cast<double*>(&parameters[0]), 
                0, 0, 1, curve_order, start_param, &end_param, &curve, 
                &point_param, &nb_unique_param, &status);
    }
    if (status != 0)
        throw std::runtime_error("cannot generate the curve");

    free(point_param);
}

void SplineBase::printCurveProperties(std::ostream& io)
{
    io << "CURVE PROPERTIES " << std::endl
	<< "  Point count  : " << curve->in    << std::endl
	<< "  Order        : " << curve->ik    << std::endl
	<< "  Dimension    : " << curve->idim  << std::endl
	<< "  Kind         : " << curve->ikind << std::endl
	<< "  Parameters   : " << start_param  << "->" << end_param << std::endl
	<< "  Length       : " << getCurveLength() << std::endl;
}

std::vector<double> SplineBase::getCoordinates() const
{
    if (!singleton.empty())
        return singleton;
    else if (!curve)
        return std::vector<double>();
    else if (isNURBS())
        return std::vector<double>(curve->rcoef, curve->rcoef + (dimension + 1) * getPointCount());
    else
        return std::vector<double>(curve->ecoef, curve->ecoef + dimension * getPointCount());
}

std::vector<double> SplineBase::getKnots() const
{
    if (!curve)
        return std::vector<double>();
    else
        return std::vector<double>(curve->et, curve->et + getPointCount() + getCurveOrder());
}

int SplineBase::getSISLCurveType() const
{
    if (curve)
        return curve->ikind;
    else return 0;
}

void SplineBase::reset(SISLCurve* new_curve)
{
    if (curve)
        freeCurve(curve);
    this->curve = new_curve;

    new_curve->cuopen = 1;
    singleton.clear();
    has_curvature_max = false;
    has_curve_length = false;
    this->curve = new_curve;

    int status;
    s1363(curve, &start_param, &end_param, &status);
    if (status != 0)
        throw std::runtime_error("cannot get the curve start & end parameters");
}

void SplineBase::reset(std::vector<double> const& coordinates, std::vector<double> const& knots, int kind)
{
    if (coordinates.empty())
    {
        clear();
        return;
    }
    else if (coordinates.size() == static_cast<size_t>(dimension))
    {
        if (curve)
            freeCurve(curve);

        start_param = end_param = 0;
        has_curvature_max = false;
        has_curve_length = true;
        curve_length = 0;
        singleton = coordinates;
        return;
    }

    int stride;
    if (kind == -1)
    {
        if (!curve)
            throw std::runtime_error("must give a 'kind' parameter to reset() since this curve is empty");

        kind   = curve->ikind;
        stride = getCoordinatesStride();
    }
    else
    {
        stride = dimension;
        if (kind == 2 || kind == 4)
            ++stride;
    }

    SISLCurve* new_curve = newCurve(coordinates.size() / stride,
            getCurveOrder(), const_cast<double*>(&knots[0]), const_cast<double*>(&coordinates[0]),
            kind, dimension, 1);
    reset(new_curve);
}

double SplineBase::findOneClosestPoint(double const* _pt, double _guess, double _geores) const
{
    if (!curve)
        return getStartParam();

    vector<double> points;
    vector< pair<double, double> > curves;
    findClosestPoints(_pt, points, curves, _geores);

    double closestPoint;
    if (points.empty())
    {
        if (curves.empty())
            throw std::logic_error("no closest point returned by findClosestPoints");
        else
            closestPoint = curves.front().first;
    }
    else
    {
        closestPoint = points.front();
        for(std::vector<double>::iterator it = points.begin() + 1; it != points.end(); ++it) 
        {
            if( fabs(*it - _guess) < fabs(closestPoint - _guess) )
                closestPoint = *it;
        }
    }

    for (std::vector< pair<double, double> >::iterator it = curves.begin();
            it != curves.end(); ++it)
    {
        if (it->first <= _guess && it->second >= _guess)
            return _guess;

        if (fabs(it->first  - _guess) < fabs(closestPoint  - _guess))
            closestPoint = it->first;

        if (fabs(it->second - _guess) < fabs(closestPoint - _guess))
            closestPoint = it->second;
    }

    return closestPoint;
}

void SplineBase::findClosestPoints(double const* ref_point, vector<double>& _result_points, vector< pair<double, double> >& _result_curves, double _geores) const
{
    if (!curve)
    {
        _result_points.push_back(0);
        return;
    }

    int points_count;
    double* points;
    int curves_count;
    SISLIntcurve** curves;

    // Finds the closest point on the curve
    int status;
    s1953(curve, const_cast<double*>(ref_point), dimension, _geores, _geores, &points_count, &points, &curves_count, &curves, &status);
    if (status != 0)
        throw std::runtime_error("failed to find the closest points");

    for (int i = 0; i < curves_count; ++i)
        _result_curves.push_back(make_pair(curves[i]->epar1[0], curves[i]->epar1[1]));
    for (int i = 0; i < points_count; ++i)
        _result_points.push_back(points[i]);

    free(curves);
    free(points);
}

double SplineBase::localClosestPointSearch(double* ref_point, double _guess, double _start, double _end, double  _geores) const
{
    if (!curve)
        return getStartParam();

    if (_end < _start)
        swap(_end, _start);

    double param;

    // Finds the closest point on the curve
    int status;
    s1774(curve, ref_point, dimension, _geores, _start, _end, _guess, &param, &status);
    if (status < 0)
        throw std::runtime_error("failed to find the closest points");

    // If the first guess is really wrong, the search might return a parameter
    // that is not between _start and _end. Validate the result value so that it
    // does not happen
    if (param < _start)
        return _start;
    else if (param > _end)
        return _end;
    else return param;
}

void SplineBase::append(SplineBase const& other)
{
    if (isEmpty())
    {
        *this = other;
        return;
    }
    else if (other.isEmpty() || other.isSingleton())
    {
        return;
    }
    else if (isSingleton())
    {
        throw std::runtime_error("cannot append a curve to a singleton");
    }

    SISLCurve* joined_curve;
    int result;
    s1715(this->curve, other.curve, 1, 0, &joined_curve, &result);
    if (result != 0)
        throw std::runtime_error("failed to join the curves");

    reset(joined_curve);
}

void SplineBase::join(SplineBase const& other, double tolerance)
{
    if (tolerance < 0)
        tolerance = 0;

    int const dim = getDimension();
    if (other.getDimension() != dim)
        throw std::runtime_error("incompatible dimensions in #join");

    std::vector<double> joining_points;
    int joining_types[4] = { 0, 0, 0, 0 };

    double* end_point = 0, * start_point = 0;

    if (isEmpty())
    {
        *this = other;
        return;
    }
    else if (other.isEmpty())
        return;
    else if (isSingleton() && other.isSingleton())
    {
        std::vector<double> line;
        line.resize(dim * 2);
        copy(singleton.begin(), singleton.end(), line.begin());
        copy(other.singleton.begin(), other.singleton.end(), line.begin() + dim);
        interpolate(line);
        return;
    }
    else if (other.isSingleton())
    {
        joining_points.resize(3 * dim);
        getPointAndTangent(&joining_points[0], getEndParam());
        copy(other.singleton.begin(), other.singleton.end(), joining_points.begin() + 2 * dim);
        for (int i = 0; i < dim; ++i)
            joining_points[i + dim] += joining_points[i];
        start_point = &joining_points[0];
        end_point   = &joining_points[2 * dim];
        joining_types[0] = 1;
        joining_types[1] = 14;
        joining_types[2] = 1;
    }
    else if (isSingleton())
    {
        joining_points.resize(3 * dim);
        copy(singleton.begin(), singleton.end(), joining_points.begin());
        other.getPointAndTangent(&joining_points[dim], getStartParam());
        for (int i = 0; i < dim; ++i)
            joining_points[i + 2 * dim] += joining_points[i + dim];
        start_point = &joining_points[0];
        end_point   = &joining_points[dim];
        joining_types[0] = 1;
        joining_types[1] = 1;
        joining_types[2] = 14;
    }
    else
    {
        joining_points.resize(4 * dim);
        // We get the tangents as well as the points, as we might need them later
        getPointAndTangent(&joining_points[0], getEndParam());
        other.getPointAndTangent(&joining_points[2 * dim], other.getStartParam());
        start_point = &joining_points[0];
        end_point   = &joining_points[2 * dim];
        joining_types[0] = 1;
        joining_types[1] = 14;
        joining_types[2] = 1;
        joining_types[3] = 14;

        for (int i = 0; i < dim; ++i)
        {
            joining_points[i + dim] += joining_points[i];
            joining_points[i + 3 * dim] += joining_points[i + 2 * dim];
        }
    }

    double dist = 0;
    for (int i = 0; i < dim; ++i)
    {
        double d = (start_point[i] - end_point[i]);
        dist += d * d;
    }
    dist = sqrt(dist);

    if (dist <= tolerance)
        return append(other);

    // Create an intermediate curve
    SISLCurve* raw_intermediate_curve = NULL;
    double end_par;
    double* gpar = NULL;
    int jnbpar;
    int ret;
    s1356(&joining_points[0], joining_points.size() / dim, dim, joining_types, 0, 0, 1, getCurveOrder(), 0,
            &end_par, &raw_intermediate_curve, &gpar, &jnbpar, &ret);
    if (ret != 0)
    {
        std::cerr << "SplineBase::join failed to generate the intermediate curve" << std::endl;
        std::cerr << "dist=" << dist << std::endl;
        std::cerr << "start=" << getStartParam() << " end=" << getEndParam() << " singleton=" << isSingleton() << std::endl;
        std::cerr << "other.start=" << other.getStartParam() << " other.end=" << other.getEndParam() << " other.singleton=" << other.isSingleton() << std::endl;
        std::cerr << "points:" << std::endl;
        for (unsigned int p = 0; p < joining_points.size() / dim; ++p)
        {
            std::cerr << p;
            for (int i = 0; i < dim; ++i)
                std::cerr << "  " << joining_points[p * dim + i];
            std::cerr << std::endl;
        }
        throw std::runtime_error("cannot generate the intermediate curve");
    }

    SplineBase intermediate_curve(getGeometricResolution(), raw_intermediate_curve);
    if (isSingleton())
        *this = intermediate_curve;
    else
        append(intermediate_curve);

    append(other);
}

void SplineBase::clear()
{
    singleton.clear();
    if (curve)
    {
        freeCurve(curve);
        curve = 0;
    }
}

void SplineBase::reverse()
{
    if (curve)
        s1706(curve);
}

bool SplineBase::testIntersection(SplineBase const& other, double resolution) const
{
    if (!curve || !other.curve)
        return false;

    int point_count;
    double* points_t1 = 0;
    double* points_t2 = 0;
    int curve_count;
    SISLIntcurve **curves = 0;
    int result;
    s1857(curve, other.curve, resolution, resolution,
            &point_count, &points_t1, &points_t2,
            &curve_count, &curves, &result);
    if (result != 0)
        throw std::runtime_error("error computing curve intersections");

    if (points_t1) free(points_t1);
    if (points_t2) free(points_t2);
    if (curves) free(curves);

    return (point_count > 0) || (curve_count > 0);
}

vector<double> SplineBase::simplify()
{
    return simplify(geometric_resolution);
}

vector<double> SplineBase::simplify(double tolerance)
{
    if (!curve)
    {
        std::vector<double> result;
        result.push_back(0);
        return result;
    }

    SISLCurve* result = NULL;
    double epsilon[3] = { tolerance, tolerance, tolerance };

    double maxerr[3];
    int status;
    s1940(curve, epsilon,
            curve_order, // derivatives
            curve_order, // derivatives
            1, // request closed curve
            10, // number of iterations
            &result, maxerr, &status);
    if (status != 0)
        throw std::runtime_error("SISL error while simplifying a curve");

    freeCurve(curve);
    curve = result;
    return vector<double>(maxerr, maxerr + 3);
}

SISLCurve const* SplineBase::getSISLCurve() const
{
    return curve;
}

SISLCurve* SplineBase::getSISLCurve()
{
    return curve;
}

base::Matrix3d SplineBase::getFrenetFrame(double _param)
{
    if (!curve)
        throw std::runtime_error("getFrenetFrame() called on an empty or degenerate curve");

    double p;    // does nothing
    double t[3], n[3], b[3]; // Frame axis

    // Finds the frenet frame
    int status;
    s2559(curve, &_param, 1, &p, t, n, b, &status);

    // Writes the frame to a matrix
    Matrix3d frame;
    frame << t[0], t[1], t[2], n[0], n[1], n[2], b[0], b[1], b[2];

    return frame;
}

double SplineBase::getHeading(double _param)
{    
    Matrix3d frame = getFrenetFrame(_param);

    // Vector if the X axis of the frame
    Vector2d Xaxis(frame(0,0),frame(0,1));
    Xaxis.normalize(); 

    // Returns the angle of Frenet X axis in Inertial frame
    return atan2(Xaxis.y(),Xaxis.x());
}

double SplineBase::headingError(double _actHeading, double _param)
{
    // Orientation error
    return angleLimit( _actHeading - getHeading(_param));
}

double SplineBase::distanceError(base::Vector3d _pt, double _param)
{
    // Error vector
    Vector3d curve_point;
    getPoint(curve_point.data(), _param);
    Vector3d error = _pt - curve_point;
    error(2) = 0.0;  // Z axis error not needed

    // Finds the angle of error vector to the Frenet X axis 
    Vector2d pt_vec(error(0),error(1));
    pt_vec.normalize(); 
    double  angle = angleLimit(atan2(pt_vec.y(),pt_vec.x()) - getHeading(_param));
    
    // Sign of the distance error depending on position of the 
    // actual robot in Frenet frame
    return (angle >= 0.0)?(error.norm()):(-error.norm());
}

base::Vector3d SplineBase::poseError(base::Vector3d _position, double _heading, double _guess)
{
    double param = findOneClosestPoint(_position.data(), _guess, getGeometricResolution());

    // Returns the error [distance error, orientation error, parameter] 
    return base::Vector3d(distanceError(_position, param), headingError(_heading, param), param);
}

