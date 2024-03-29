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
    Vector3(const Vector3& rhs) = default;
    explicit Vector3(float in)
        : x(in)
        , y(in)
        , z(in)
    {}
    Vector3(float inX, float inY, float inZ)
        : x(inX)
        , y(inY)
        , z(inZ)
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

    Vector3 operator*(const Vector3& rhs) const
    {
        return { x * rhs.x, y * rhs.y, z * rhs.z };
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

class Material
{
public:
    Vector3 color;
    float   roughness;
};

class Sphere
{
public:
    Vector3         center;
    float           radius;
    const Material* material;
};

template< typename T >
T Lerp(T val0, T val1, float t)
{
    return val0 + (val1 - val0) * t;
}

float Saturate(float in)
{
    return in < 0.0f ? 0.0f : in > 1.0f ? 1.0f : in;
}

float Max(float a, float b)
{
    return a > b ? a : b;
}

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

bool TraceRayOcclusion(const Ray& ray, const std::vector<Sphere>& spheres)
{
    for (auto& sphere : spheres)
    {
        auto intersections = Intersect(ray, sphere);
        if (intersections.size())
        {
            return true;
        }
    }
    return false;
}

const Vector3 LightDir = Vector3{ 1.0f, 1.0f, -1.0f }.Normalize();
const Vector3 SkyCol = Vector3{ 0.75f, 0.75f, 1.0f };

float normRand()
{
    return rand() % 1000 * 0.001f - 1.0f;
}

Vector3 RandomVector(Vector3 axis, float variance)
{
    float xRand = normRand() * variance;
    float yRand = normRand() * variance;
    float zRand = normRand() * variance;

    return (axis + Vector3{ xRand, yRand, zRand }).Normalize();
}

Vector3 TraceRayRecurse(const Ray& ray, const std::vector<Sphere>& spheres, const Vector3& cameraPosition, int recurse)
{
    float closestDist = FLT_MAX;
    Vector3 normal{ 0.0f, 1.0f, 0.0f };
    Vector3 intersection{ 0.0f, 0.0f, 0.0f };
    Vector3 color{ 0.0f, 0.0f, 0.0f };
    Vector3 eye{ 0.0f, 0.0f, 0.0f };
    float roughness = 0.0f;

    for (auto& sphere : spheres)
    {
        auto intersections = Intersect(ray, sphere);
        if (intersections.size())
        {
            Vector3 eyeVec = intersections[0] - cameraPosition;
            float dist = eyeVec.Length();
            if (dist < closestDist)
            {
                normal = (intersections[0] - sphere.center).Normalize();
                intersection = intersections[0];
                closestDist = dist;
                color = sphere.material->color;
                roughness = sphere.material->roughness;
                eye = eyeVec.Normalize();
            }
        }
    }

    float diffuse = normal.Dot(LightDir);
    diffuse = diffuse >= 0.0f ? diffuse : 0.0f;

    Vector3 half = ((eye * -1.0f) + LightDir).Normalize();
    float specular = normal.Dot(half);
    specular = specular < 0.0f ? 0.0f : specular;

    const float specularExp = 128.0f;
    specular = powf(specular, specularExp);

    Vector3 origin = intersection + normal * 0.001f;

    float occlusion = TraceRayOcclusion({ origin, LightDir }, spheres) ? 0.0f : 1.0f;
    diffuse *= occlusion;
    specular *= occlusion;

    const float ambient = 0.15f;
    Vector3 diffuseColor = color * Max(diffuse, ambient);

    if (closestDist == FLT_MAX)
    {
        return SkyCol;
    }
    else
    {
        if (recurse < 8)
        {
            Vector3 result{ 0.0f, 0.0f, 0.0f };

            const int numSamples = 1;
            for (int i = 0; i < numSamples; ++i)
            {
                result = TraceRayRecurse({ origin, normal }, spheres, cameraPosition, recurse + 1);
                //result = result + diffuseColor * TraceRayRecurse({ origin, RandomVector(LightDir, 0.125f) }, spheres, cameraPosition, recurse + 1);
            }

            result = result * (1.0f / (float)numSamples);

            return Lerp(diffuseColor * roughness + result * (1.0f - roughness), Vector3(1.0f, 1.0f, 1.0f), specular);
            //return result;
        }
        else
        {
            return Lerp(diffuseColor, Vector3(1.0f, 1.0f, 1.0f), specular);
        }
    }
}

void Render(int width, int height, HDC hdc)
{
    std::vector<Material> materials;
    materials.emplace_back(Material{ Vector3{0.75f, 1.0f, 0.75f}, 0.975f });
    materials.emplace_back(Material{ Vector3{0.0f, 0.0f, 1.0f}, 0.9f });
    materials.emplace_back(Material{ Vector3{1.0f, 0.0f, 0.0f}, 0.9f });
    materials.emplace_back(Material{ Vector3{0.0f, 1.0f, 0.0f}, 1.0f });
    materials.emplace_back(Material{ Vector3{1.0f, 1.0f, 0.0f}, 0.985f });
    materials.emplace_back(Material{ Vector3{0.0f, 1.0f, 1.0f}, 0.985f });
    materials.emplace_back(Material{ Vector3{1.0f, 0.0f, 1.0f}, 0.985f });
    materials.emplace_back(Material{ Vector3{1.0f, 1.0f, 1.0f}, 0.95f });
    materials.emplace_back(Material{ Vector3{0.25f, 0.25f, 1.0f}, 0.95f });
    materials.emplace_back(Material{ Vector3{1.0f, 0.25f, 0.25f}, 0.95f });
    materials.emplace_back(Material{ Vector3{0.5f, 1.0f, 0.25f}, 0.95f });
    materials.emplace_back(Material{ Vector3{1.0f, 1.0f, 0.25f}, 0.9f });
    materials.emplace_back(Material{ Vector3{0.25f, 1.0f, 1.0f}, 0.9f });
    materials.emplace_back(Material{ Vector3{1.0f, 0.25f, 1.0f}, 0.9f });

    srand(43);
    std::vector<Sphere> spheres;
    for (int i = 0; i < 40; ++i)
    {
        float randX = static_cast<float>((rand() % 1000 - 500)) * 0.01f;
        float randY = static_cast<float>((rand() % 500)) * 0.01f;
        float randZ = static_cast<float>((rand() % 1000)) * 0.01f;
        float randRadius = static_cast<float>((rand() % 1000)) * 0.00125f;
        uint32_t randMaterialIndex = i % materials.size();
        spheres.emplace_back(Sphere{ { randX, randY, randZ }, { randRadius }, { materials.data() + randMaterialIndex } });
    }
    spheres.emplace_back(Sphere{ { 0.0f, -1000.0f, 5.0f }, { 999.0f }, materials.data() });

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
            COLORREF fragmentColor = RGB(static_cast<BYTE>(Saturate(color.x) * 255.0f), static_cast<BYTE>(Saturate(color.y) * 255.0f), static_cast<BYTE>(Saturate(color.z) * 255.0f));
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
