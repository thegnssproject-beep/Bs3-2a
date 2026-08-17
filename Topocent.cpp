#include "Topocent.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

void Topocent::togeod(double a, double finv, double X, double Y, double Z, double& phi, double& lambda, double& h)
{
    double f = (finv != 0.0) ? (1.0 / finv) : 0.0;
    double e2 = 2.0 * f - f * f;

    lambda = std::atan2(Y, X);
    double p = std::sqrt(X * X + Y * Y);

    if (p < 1e-10) {
        phi = (Z >= 0.0) ? (M_PI / 2.0) : (-M_PI / 2.0);
        h = std::abs(Z) - (a * (1.0 - f));
        return;
    }

    phi = std::atan2(Z, p * (1.0 - e2));
    double N = a;
    h = 0.0;

    for (int i = 0; i < 5; ++i) {
        double sinPhi = std::sin(phi);
        N = a / std::sqrt(1.0 - e2 * sinPhi * sinPhi);
        h = p / std::cos(phi) - N;
        phi = std::atan2(Z, p * (1.0 - e2 * (N / (N + h))));
    }
}

void Topocent::compute(const std::vector<double>& X, const std::vector<double>& dx, double& Az, double& El, double& D)
{
    double dtr = M_PI / 180.0;

    double phi = 0.0, lambda = 0.0, h = 0.0;
    togeod(6378137.0, 298.257222101, X[0], X[1], X[2], phi, lambda, h);

    double cl = std::cos(lambda);
    double sl = std::sin(lambda);
    double cb = std::cos(phi);
    double sb = std::sin(phi);

    // Transformation matrix F from ECEF to Topocentric (ENU)
    // F = [ -sl    -sb*cl    cb*cl
    //        cl    -sb*sl    cb*sl
    //         0      cb        sb  ]
    double F[3][3] = {
        {-sl,       -sb * cl,   cb * cl},
        { cl,       -sb * sl,   cb * sl},
        { 0.0,       cb,        sb     }
    };

    // local_vector = F^T * dx  (ENU coordinates)
    double E = F[0][0] * dx[0] + F[1][0] * dx[1] + F[2][0] * dx[2];
    double N = F[0][1] * dx[0] + F[1][1] * dx[1] + F[2][1] * dx[2];
    double U = F[0][2] * dx[0] + F[1][2] * dx[1] + F[2][2] * dx[2];

    double hor_dis = std::sqrt(E * E + N * N);

    if (hor_dis < 1.e-10) {
        Az = 0.0;
        El = (U >= 0.0) ? 90.0 : -90.0;
    }
    else {
        Az = std::atan2(E, N) / dtr;
        El = std::atan2(U, hor_dis) / dtr;
    }

    if (Az < 0.0) {
        Az += 360.0;
    }

    D = std::sqrt(dx[0] * dx[0] + dx[1] * dx[1] + dx[2] * dx[2]);
}