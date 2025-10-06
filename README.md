Boids That Sprint
 
This is my honours project, heavily modified from a pre-existing tutorial series that covers the basics of creating a Directx12 renderer. Boids That Sprint aimed to investigate the benefits of utilizing GPGPU and asynchronous compute techniques within Directx12 to improve runtime performance of a simple boids model. 

Asynchronous compute techniques involved executing both the render and compute pipelines simultaneously, resulting in higher performance benefits with lower numbers of entities due to increased occupancy of the GPU. However, at larger boids entities of around 100,000+, the performance benefit drops off substantially due to the GPGPU demands of the algorithm, resulting in fewer GPU resources being available for the render pipeline to utilize simultaneously.

Link to the original repository here: https://github.com/jpvanoosten/LearningDirectX12/tree/v0.0.4 

Link to tutorial series followed: https://www.3dgep.com/learning-directx-12-1/ by Jeremiah, 2017 
