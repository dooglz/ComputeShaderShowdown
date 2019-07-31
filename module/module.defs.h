#ifdef CLMODULEDEFS
typedef void(__cdecl* go_cl)(cl::Platform&, cl::Device&, size_t);
#endif // CLMODULEDEFS
#ifdef CLMODULEDEFS_EXPORT
typedef void(__cdecl* go_cl)(cl::Platform&, cl::Device&, size_t);
#endif // CLMODULEDEFS_EXPORT

#ifdef CUDAMODULEDEFS
typedef void(__cdecl* go_cu)(size_t);
#endif // CUDAMODULEDEFS

#ifdef DX12MODULEDEFS
typedef void(__cdecl* go_dx)(size_t);
#endif // DX12MODULEDEFS

#ifdef VULKANMODULEDEFS
typedef void(__cdecl* go_vk)(size_t);
#endif // VULKANMODULEDEFS
