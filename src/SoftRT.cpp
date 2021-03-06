// SoftRT.cpp

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <vector>

const float kPi = 3.1415927f;
const float kEpsilon = 0.00001f;

class Vector3
{
public:
    Vector3() = default;
    Vector3(float x_, float y_, float z_)
        : x(x_)
        , y(y_)
        , z(z_)
    {}

    float Dot(const Vector3& rhs) const
    {
        return x * rhs.x + y * rhs.y + z * rhs.z;
    }

    float Length() const
    {
        return sqrtf(Dot(*this));
    }

    Vector3 operator-(const Vector3& rhs) const
    {
        return { x - rhs.x, y - rhs.y, z - rhs.z };
    }

    Vector3 operator+(const Vector3& rhs) const
    {
        return { x + rhs.x, y + rhs.y, z + rhs.z };
    }

    Vector3 operator*(float rhs) const
    {
        return { x * rhs, y * rhs, z * rhs };
    }

    Vector3 Normalize() const
    {
        return *this * (1.0f / Length());
    }

    float Distance(const Vector3& rhs)
    {
        return (*this - rhs).Length();
    }

    float x;
    float y;
    float z;
};

class Ray
{
public:
    Vector3 origin;
    Vector3 direction;
};

class Sphere
{
public:
    Vector3 center;
    float   radius;
};

std::vector<Vector3> Intersect(const Ray& ray, const Sphere& sphere)
{
    // Calculate discriminant
    Vector3 rayOriginToSphereCenter = ray.origin - sphere.center;
    float a = ray.direction.Dot(ray.direction);
    float b = 2.0f * ray.direction.Dot(rayOriginToSphereCenter);
    float c = rayOriginToSphereCenter.Dot(rayOriginToSphereCenter) - sphere.radius * sphere.radius;
    float discriminant = b * b - 4.0f * a * c;

    std::vector<Vector3> result;
    if (discriminant < 0.0f)
    {
        return result;
    }
    else
    {
        // Solve quadratic for real roots
        float t0 = (-1.0f * b + sqrtf(discriminant)) / (2.0f * a);
        if (t0 >= 0.0f)
        {
            result.emplace_back(ray.origin + ray.direction * t0);
        }

        if (discriminant > kEpsilon)
        {
            float t1 = (-1.0f * b - sqrtf(discriminant)) / (2.0f * a);
            if (t1 >= 0.0f)
            {
                result.emplace_back(ray.origin + ray.direction * t1);

                // Order by distance; smallest positive root is closest
                if ((t0 >= 0.0f && t1 >= 0.0f) && (t1 < t0))
                {
                    std::swap(result[0], result[1]);
                }
            }
        }

        return result;
    }
}

Vector3 TraceRay(const Ray& ray, const std::vector<Sphere>& spheres, const Vector3& cameraPosition)
{
    float closestDist = FLT_MAX;
    Vector3 normal{ 0.0f, 1.0f, 0.0f };

    for (auto& sphere : spheres)
    {
        auto intersections = Intersect(ray, sphere);
        if (intersections.size())
        {
            float dist = (intersections[0] - cameraPosition).Length();
            if (dist < closestDist)
            {
                normal = (intersections[0] - sphere.center).Normalize();
                closestDist = dist;
            }
        }
    }

    if (closestDist == FLT_MAX)
    {
        return Vector3(0.35f, 0.45f, 1.0f);
    }
    else
    {
        return Vector3(normal.x, normal.y, normal.z);
    }
}

Vector3 TraceRayRecurse(const Ray& ray, const std::vector<Sphere>& spheres, const Vector3& cameraPosition, int recurse)
{
    float closestDist = FLT_MAX;
    Vector3 normal{ 0.0f, 1.0f, 0.0f };
    Vector3 intersection{ 0.0f, 0.0f, 0.0f };

    for (auto& sphere : spheres)
    {
        auto intersections = Intersect(ray, sphere);
        if (intersections.size())
        {
            float dist = (intersections[0] - cameraPosition).Length();
            if (dist < closestDist)
            {
                normal = (intersections[0] - sphere.center).Normalize();
                intersection = intersections[0];
                closestDist = dist;
            }
        }
    }

    float diffuse = normal.Dot(Vector3(1.0f, 1.0f, -1.0f).Normalize());
    diffuse = diffuse >= 0.0f ? diffuse : 0.0f;
    Vector3 diffuseColor{ diffuse, diffuse, diffuse };

    if (closestDist == FLT_MAX)
    {
        return (diffuseColor + Vector3{ 0.35f, 0.45f, 1.0f }) * 0.5f;
    }
    else
    {
        if (recurse < 10)
        {
            Vector3 result{ 0.0f, 0.0f, 0.0f };

            const int numSamples = 1;
            for (int i = 0; i < numSamples; ++i)
            {
                Vector3 origin = intersection + normal * 0.001f;
                result = TraceRayRecurse({ origin, normal }, spheres, cameraPosition, recurse + 1);
            }

            return (diffuseColor + result) * 0.5f;
        }
        else
        {
            return diffuseColor;
        }
    }
}

void Render(int width, int height, HDC hdc)
{
    std::vector<Sphere> spheres;

    for (int i = 0; i < 20; ++i)
    {
        float randX = static_cast<float>((rand() % 1000 - 500)) * 0.01f;
        float randY = static_cast<float>((rand() % 500)) * 0.01f;
        float randZ = static_cast<float>((rand() % 1000)) * 0.01f;
        float randRadius = static_cast<float>((rand() % 1000)) * 0.00125f;
        spheres.emplace_back(Sphere{ { randX, randY, randZ }, { randRadius } });
    }

    spheres.emplace_back(Sphere{ { 0.0f, -1000.0f, 5.0f }, { 999.0f } });

    float dx = 2.0f / static_cast<float>(width);
    float dy = 2.0f / static_cast<float>(height);

    Vector3 camPos{ 0.0f, 0.0f, -2.0f };

    for (int i = 0; i < width; ++i)
    {
        for (int j = 0; j < height; ++j)
        {
            Ray ray;
            ray.origin = camPos;

            Vector3 nearPlanePos;
            nearPlanePos.x = -1.0f + dx * static_cast<float>(i);
            nearPlanePos.y = 1.0f - dy * static_cast<float>(j);
            nearPlanePos.z = 0.0f;
            ray.direction = nearPlanePos - camPos;
            Vector3 color = TraceRayRecurse(ray, spheres, camPos, 0);
            COLORREF fragmentColor = RGB(static_cast<char>(color.x * 255.0f), static_cast<char>(color.y * 255.0f), static_cast<char>(color.z * 255.0f));
            SetPixel(hdc, i, j, fragmentColor);
        }
    }
}

// Butchered win32 boilerplate appwizard code follows:
HINSTANCE hInst;
CHAR* szWindowClass = "SoftRT";

// Forward declarations of functions included in this code module:
ATOM                MyRegisterClass(HINSTANCE hInstance);
BOOL                InitInstance(HINSTANCE, int);
LRESULT CALLBACK    WndProc(HWND, UINT, WPARAM, LPARAM);

int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
                     _In_opt_ HINSTANCE hPrevInstance,
                     _In_ LPWSTR    lpCmdLine,
                     _In_ int       nCmdShow)
{
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);

    // Initialize global strings
    MyRegisterClass(hInstance);

    // Perform application initialization:
    if (!InitInstance (hInstance, nCmdShow))
    {
        return FALSE;
    }

    MSG msg;

    // Main message loop:
    while (GetMessage(&msg, nullptr, 0, 0))
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return (int) msg.wParam;
}

ATOM MyRegisterClass(HINSTANCE hInstance)
{
    WNDCLASSEX wcex;

    wcex.cbSize = sizeof(WNDCLASSEX);

    wcex.style          = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc    = WndProc;
    wcex.cbClsExtra     = 0;
    wcex.cbWndExtra     = 0;
    wcex.hInstance      = hInstance;
    wcex.hIcon          = LoadIcon(hInstance, IDI_APPLICATION);
    wcex.hCursor        = LoadCursor(nullptr, IDC_ARROW);
    wcex.hbrBackground  = (HBRUSH)(COLOR_WINDOW+1);
    wcex.lpszMenuName   = NULL;
    wcex.lpszClassName  = szWindowClass;
    wcex.hIconSm        = NULL;

    return RegisterClassEx(&wcex);
}

BOOL InitInstance(HINSTANCE hInstance, int nCmdShow)
{
   hInst = hInstance; // Store instance handle in our global variable

   HWND hWnd = CreateWindow(szWindowClass, "SoftRT", WS_OVERLAPPEDWINDOW,
      100, 100, 1024, 1024, nullptr, nullptr, hInstance, nullptr);

   if (!hWnd)
   {
      return FALSE;
   }

   ShowWindow(hWnd, nCmdShow);
   UpdateWindow(hWnd);

   return TRUE;
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    case WM_PAINT:
        {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hWnd, &ps);
            // TODO: Add any drawing code that uses hdc here...
            RECT winRect;
            GetWindowRect(hWnd, &winRect);
            Render(winRect.right - winRect.left, winRect.bottom - winRect.top, hdc);
            EndPaint(hWnd, &ps);
        }
        break;
    case WM_DESTROY:
        PostQuitMessage(0);
        break;
    default:
        return DefWindowProc(hWnd, message, wParam, lParam);
    }
    return 0;
}
