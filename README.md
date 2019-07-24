## Compute Shader Showdown

### A.K.A _Helloworld.comp_

This is a project to have a minimum working example of compute workloads across APIs.
At present the app loads a big array of ints into the GPU, multiplies them, and reads them back.

This is also used as a testbed for looking at profiling tools and what they can offer to compute only applications

Currently implemented APIs:
- Vulkan
- DX12 Compute
- Cuda
- OpenCL

With helper code to work with the following Profiling tools
- RenderDoc
- PIX
- GPUView (via windows EventWriteString)

Work currently ongoing to add
- Radeon GPU Profiler (RGP)
- Radeon Compute PRofiler (RCP)
- Intel Tools
- Nvidia NSight
- anything else I find


## CONTRIBUTING
I've had to write this code fast and on a deadline, it's horrible, don't hate me.
Also See [CONTRIBUTING.md]()