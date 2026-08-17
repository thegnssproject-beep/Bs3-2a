#pragma once

#include <vector>
#include <cmath>

class Topocent
{
public:
    // Transforms ECEF vector dx with origin X into local topocentric coordinates
    // Inputs:
    //   X  - Receiver ECEF coordinates [X, Y, Z] (meters)
    //   dx - ECEF vector [dX, dY, dZ] (e.g., Satellite position minus Receiver position)
    // Outputs:
    //   Az - Azimuth angle from North, clockwise (degrees, 0 to 360)
    //   El - Elevation angle (degrees, -90 to +90)
    //   D  - Euclidean distance / vector length (meters)
    static void compute(const std::vector<double>& X, const std::vector<double>& dx, double& Az, double& El, double& D);

private:
    // Helper to convert ECEF to geodetic latitude and longitude (equivalent to togeod)
    static void togeod(double a, double finv, double X, double Y, double Z, double& phi, double& lambda, double& h);
};