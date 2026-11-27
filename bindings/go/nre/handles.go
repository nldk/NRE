package nre

/*
#include "nre.h"
*/
import "C"

// Handles are copyable values (index + generation, see nre.h). Their fields are private; compare them
// with ==. The zero value is the empty handle, e.g. Texture{} for "no texture" or Cubemap{} for "no skybox".

// Shader is NreShader: a compiled SPIR-V shader stage (CreateShader). The zero value is the empty handle.
type Shader struct{ index, generation uint32 }

func (h Shader) c() C.NreShader {
	return C.NreShader{index: C.uint32_t(h.index), generation: C.uint32_t(h.generation)}
}

func shaderFromC(h C.NreShader) Shader { return Shader{uint32(h.index), uint32(h.generation)} }

// Pipeline is NrePipeline: a graphics pipeline made of a vertex and a fragment shader (CreatePipeline). The zero value is the empty handle.
type Pipeline struct{ index, generation uint32 }

func (h Pipeline) c() C.NrePipeline {
	return C.NrePipeline{index: C.uint32_t(h.index), generation: C.uint32_t(h.generation)}
}

func pipelineFromC(h C.NrePipeline) Pipeline { return Pipeline{uint32(h.index), uint32(h.generation)} }

// Model is NreModel: a loaded model file: meshes, GPU buffers, materials (CreateModel). Draw it through instances or DrawModel. The zero value is the empty handle.
type Model struct{ index, generation uint32 }

func (h Model) c() C.NreModel {
	return C.NreModel{index: C.uint32_t(h.index), generation: C.uint32_t(h.generation)}
}

func modelFromC(h C.NreModel) Model { return Model{uint32(h.index), uint32(h.generation)} }

// Instance is NreInstance: a placed copy of a model with its own transform (CreateInstance). The zero value is the empty handle.
type Instance struct{ index, generation uint32 }

func (h Instance) c() C.NreInstance {
	return C.NreInstance{index: C.uint32_t(h.index), generation: C.uint32_t(h.generation)}
}

func instanceFromC(h C.NreInstance) Instance { return Instance{uint32(h.index), uint32(h.generation)} }

// Light is NreLight: a directional or point light (CreateDirectionalLight, CreatePointLight). The zero value is the empty handle.
type Light struct{ index, generation uint32 }

func (h Light) c() C.NreLight {
	return C.NreLight{index: C.uint32_t(h.index), generation: C.uint32_t(h.generation)}
}

func lightFromC(h C.NreLight) Light { return Light{uint32(h.index), uint32(h.generation)} }

// Scene is NreScene: a set of instances and lights plus the camera, sun and skybox (CreateScene). The zero value is the empty handle.
type Scene struct{ index, generation uint32 }

func (h Scene) c() C.NreScene {
	return C.NreScene{index: C.uint32_t(h.index), generation: C.uint32_t(h.generation)}
}

func sceneFromC(h C.NreScene) Scene { return Scene{uint32(h.index), uint32(h.generation)} }

// Texture is NreTexture: an image loaded as a texture, colour (sRGB) or data (CreateTexture, CreateTextureFull). The zero value is the empty handle.
type Texture struct{ index, generation uint32 }

func (h Texture) c() C.NreTexture {
	return C.NreTexture{index: C.uint32_t(h.index), generation: C.uint32_t(h.generation)}
}

func textureFromC(h C.NreTexture) Texture { return Texture{uint32(h.index), uint32(h.generation)} }

// Material is NreMaterial: a material, owned by a model's slots (CreateMaterial, GetModelMaterials). The zero value is the empty handle.
type Material struct{ index, generation uint32 }

func (h Material) c() C.NreMaterial {
	return C.NreMaterial{index: C.uint32_t(h.index), generation: C.uint32_t(h.generation)}
}

func materialFromC(h C.NreMaterial) Material { return Material{uint32(h.index), uint32(h.generation)} }

// Cubemap is NreCubemap: six images forming a cube, for the skybox (CreateCubemap). The zero value is the empty handle.
type Cubemap struct{ index, generation uint32 }

func (h Cubemap) c() C.NreCubemap {
	return C.NreCubemap{index: C.uint32_t(h.index), generation: C.uint32_t(h.generation)}
}

func cubemapFromC(h C.NreCubemap) Cubemap { return Cubemap{uint32(h.index), uint32(h.generation)} }
