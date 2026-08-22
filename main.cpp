#include <iostream>
#include <fstream>
#include <cmath>
#include <algorithm>
#include <vector>
#include <string>

struct Vector3
{
    double x;
    double y;
    double z;
};

struct Ray
{
    Vector3 origin;
    Vector3 direction;
};

struct Sphere
{
    Vector3 center;
    double radius;
    Vector3 color;
    bool emissive;
};

struct Plane
{
    Vector3 point;
    Vector3 normal;
    Vector3 color;
};

struct SphereHit
{
    double t;
    int sphereIndex;
};

struct Camera
{
    Vector3 origin;
    double viewportWidth;
    double viewportHeight;
    double focalLength;
};

Vector3 add(const Vector3& a, const Vector3& b)
{
    return Vector3{a.x + b.x, a.y + b.y, a.z + b.z};
}

Vector3 subtract(const Vector3& a, const Vector3& b)
{
    return Vector3{a.x - b.x, a.y - b.y, a.z - b.z};
}

Vector3 multiply(const Vector3& v, double s)
{
    return Vector3{v.x * s, v.y * s, v.z * s};
}

double dot(const Vector3& a, const Vector3& b)
{
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

double length(const Vector3& a)
{
    return std::sqrt(dot(a, a));
}

Vector3 normalize(const Vector3& v)
{
    double storedLength = length(v);
    if (storedLength == 0.0)
    {
        return Vector3{0.0, 0.0, 0.0};
    }

    return multiply(v, 1.0 / storedLength);
}

Vector3 pointAt(const Ray& ray, double t)
{
    Vector3 scaledDirection = multiply(ray.direction, t);
    return add(ray.origin, scaledDirection);

}

double hitSphere(const Ray& ray, const Sphere& sphere)
{
    Vector3 oc = subtract(ray.origin, sphere.center);
    double a = dot(ray.direction, ray.direction);
    double b = dot(oc, ray.direction) * 2;
    double c = dot(oc, oc) - sphere.radius * sphere.radius;

    double discriminant = b * b - 4 * a * c;

    if (discriminant < 0.0)
    {
        return -1.0;
    }

    double squareRoot = std::sqrt(discriminant);

    double tNear = (-b - squareRoot) / (2 * a);
    double tFar = (-b + squareRoot) / (2 * a);

    if (tNear > 0.0)
    {
        return tNear;
    }

    if (tFar > 0.0)
    {
        return tFar;
    }

    return -1.0;
}

SphereHit findClosestSphere(
        const Ray& ray,
        const std::vector<Sphere>& spheres)
{
    SphereHit closest{-1.0, -1};

    for (int index = 0; index < static_cast<int>(spheres.size()); index++)
    {
        double t = hitSphere(ray, spheres[index]);

        if (t > 0.0 && (closest.t < 0.0 || t < closest.t))
        {
            closest.t = t;
            closest.sphereIndex = index;
        }
    }

    return closest;
}

bool isShadowed(
    const Ray& shadowRay,
    double lightDistance,
    const std::vector<Sphere>& spheres)
{
   for (const Sphere& sphere : spheres)
   {
        if (sphere.emissive)
        {
            continue;
        }
        double t = hitSphere(shadowRay, sphere);

        if (t > 0.0 && t < lightDistance)
        {
            return true;
        }
   }
   return false;
}

double hitPlane (const Ray& ray, const Plane& plane)
{
    double denominator = dot(ray.direction, plane.normal);
    Vector3 direction = subtract(plane.point, ray.origin);
    double numerator = dot(direction, plane.normal);

    if (std::abs(denominator) < 0.000001)
    {
        return -1.0;
    }

    double t = numerator / denominator;
    if (t > 0.0)
    {
        return t;
    }
    return -1.0;
}

Ray makeCameraRay(
    const Camera& camera,
    double u,
    double v
)
{
    double viewportX = (u - 0.5) * camera.viewportWidth;
    double viewportY = (0.5 - v) * camera.viewportHeight;

    Vector3 viewportOffset {
        viewportX,
        viewportY,
        -camera.focalLength
    };

    Vector3 viewportPoint = add(camera.origin, viewportOffset);
    Vector3 cameraDirection = subtract(viewportPoint, camera.origin);
    Vector3 normCameraDirection = normalize(cameraDirection);

    Ray cameraRay{
        camera.origin,
        normCameraDirection
    };
    return cameraRay;
}

Vector3 traceRay(
    const Ray& ray,
    const std::vector<Sphere>& spheres,
    const Plane& plane,
    bool planeEnabled,
    const Vector3& lightPosition,
    const Vector3& backgroundColor)
{
    SphereHit sphereHit = findClosestSphere(ray, spheres);
    double planeT = 
        planeEnabled ? hitPlane(ray, plane) : -1.0;

    bool sphereIsClosest =
        sphereHit.t > 0.0 &&
        (planeT < 0.0 || sphereHit.t < planeT);

    if (sphereIsClosest)
    {
        const Sphere& hitObject =
            spheres[sphereHit.sphereIndex];
        
        if (hitObject.emissive)
        {
            return hitObject.color;
        }

        Vector3 hitPoint = pointAt(ray, sphereHit.t);
        Vector3 outward = subtract(hitPoint, hitObject.center);
        Vector3 normal = normalize(outward);

        double epsilon = 0.0001;
        Vector3 offset = multiply(normal, epsilon);
        Vector3 shadowOrigin = add(hitPoint, offset);

        Vector3 toLight = subtract(lightPosition, hitPoint);
        Vector3 lightDirection = normalize(toLight);

        Ray shadowRay{
            shadowOrigin,
            lightDirection
        };

        double lightDistance = length(toLight);
        bool inShadow = 
            isShadowed(shadowRay, lightDistance, spheres);

        double diffuseIntensity = dot(normal, lightDirection);
        diffuseIntensity = std::max(0.0, diffuseIntensity);

        if (inShadow)
        {
            diffuseIntensity = 0.0;
        }

        return multiply(hitObject.color, diffuseIntensity);
    }

    if (planeT > 0.0)
    {
        Vector3 planeHitPoint = pointAt(ray, planeT);
        int checkerX = static_cast<int>(std::floor(planeHitPoint.x));
        int checkerZ = static_cast<int>(std::floor(planeHitPoint.z));
        bool useLightSquare = 
            (checkerX + checkerZ) % 2 == 0;

        Vector3 checkerColor;
        if (useLightSquare)
        {
            checkerColor = plane.color;
        }
        else
        {
            checkerColor = multiply(plane.color, 0.25);
        }

        Vector3 planeNormal = plane.normal;
        Vector3 toLight = subtract(lightPosition, planeHitPoint);
        Vector3 planeLightDirection = normalize(toLight);

        double planeDiffuseIntensity = dot(planeNormal, planeLightDirection);
        planeDiffuseIntensity = std::max(0.0, planeDiffuseIntensity);

        double epsilon = 0.0001;
        Vector3 planeOffset = multiply(planeNormal, epsilon);
        Vector3 planeShadowOrigin = add(planeHitPoint, planeOffset);

        Ray planeShadowRay{
            planeShadowOrigin,
            planeLightDirection
        };

        double planeLightDistance = length(toLight);
        bool planeInShadow =
            isShadowed(planeShadowRay, planeLightDistance, spheres);

        if (planeInShadow)
        {
            planeDiffuseIntensity = 0.0;
        }

        return multiply(checkerColor, planeDiffuseIntensity);
    }
    
    return backgroundColor;
}

int main() 
{
    const int windowWidth = 256;
    const int windowHeight = 256;
    const int maxChannelValue = 255;
    const int samplesPerAxis = 2;

    Camera camera{
        {0, 0,0 },
        2.0,
        2.0,
        1.0
    };

    std::vector<Sphere> spheres {
        Sphere{{0, 0, -3}, 1, {255, 190, 40}, true},
        Sphere{{1.3, -0.5, -3.0}, 0.5, {40, 80, 255}, false}
    };

    Plane plane {
        {0, -1, 0},
        {0, 1, 0},
        {160, 160, 160}
    };

    Vector3 lightPosition = spheres[0].center;

    int frameCount = 120;
    double orbitRadius = 1.8;
    double twoPi = 2.0 * std::acos(-1.0);

    for (int frame = 0; frame < frameCount; frame++)
    {
        double theta = twoPi * (static_cast<double>(frame) / frameCount);

        spheres[1].center.x = spheres[0].center.x + orbitRadius * std::cos(theta);
        spheres[1].center.y = spheres[0].center.y + orbitRadius * std::sin(theta);

        std::string filename = 
            "frame_" + std::to_string(frame) + ".ppm";

        std::ofstream outputFile(filename);

        if(!outputFile.is_open())
        {
            std::cerr << "Error opening file" << std::endl;
            return 1;
        }
        outputFile << "P3" << "\n";
        outputFile << windowWidth << " " << windowHeight << "\n";
        outputFile << maxChannelValue << "\n";

        for (int row = 0; row < windowHeight; row++)
        {
            for (int col = 0; col < windowWidth; col++)
            {
                Vector3 accumulatedColor {0.0, 0.0, 0.0};

                for (int sampleY = 0; sampleY < samplesPerAxis; sampleY++)
                {
                    for (int sampleX = 0; sampleX < samplesPerAxis; sampleX++)
                    {
                        double offsetX =
                            (static_cast<double>(sampleX) + 0.5) / samplesPerAxis;
                        double offsetY =
                            (static_cast<double>(sampleY) + 0.5) / samplesPerAxis;

                        double u = (col + offsetX) / windowWidth;
                        double v = (row + offsetY) / windowHeight;

                        Ray cameraRay = makeCameraRay(camera, u, v);

                        Vector3 horizonColor {4.0, 6.0, 18.0};
                        Vector3 topColor {0.0, 0.0, 4.0};

                        double skyT = 1.0 - v;
                        Vector3 horizonContribution = multiply(horizonColor, 1 - skyT);
                        Vector3 topContribution = multiply(topColor, skyT);
                        Vector3 backgroundColor =
                            add(horizonContribution, topContribution);
                        
                        Vector3 pixelColor = traceRay(
                            cameraRay,
                            spheres,
                            plane,
                            false,
                            lightPosition,
                            backgroundColor);

                        accumulatedColor = add(accumulatedColor, pixelColor);
                    }
                }
                double totalSamples = samplesPerAxis * samplesPerAxis;
                Vector3 averageColor = multiply(accumulatedColor, 1.0 / totalSamples);

                outputFile <<
                    static_cast<int> (averageColor.x) << " " <<
                    static_cast<int> (averageColor.y) << " " <<
                    static_cast<int> (averageColor.z) << "\n";
            }   
        }
        outputFile.close();
    }

    return 0;
}
