CFLAGS=-O0

bin:
	mkdir -p bin

cubemap.vert.spv: bin cubemap.vert.glsl
	glslc $(CFLAGS) -fshader-stage=vert cubemap.vert.glsl -o bin/cubemap.vert.spv

cubemap.frag.spv: bin cubemap.frag.glsl
	glslc $(CFLAGS) -fshader-stage=frag cubemap.frag.glsl -o bin/cubemap.frag.spv

tri_ubo.vert.spv: bin tri_ubo.vert.glsl
	glslc $(CFLAGS) -fshader-stage=vert tri_ubo.vert.glsl -o bin/tri_ubo.vert.spv

tri_ubo.frag.spv: bin tri_ubo.frag.glsl
	glslc $(CFLAGS) -fshader-stage=frag tri_ubo.frag.glsl -o bin/tri_ubo.frag.spv

cubemap: cubemap.vert.spv cubemap.frag.spv

tri_ubo: tri_ubo.vert.spv tri_ubo.frag.spv

clean:
	rm -rf bin
	rm -f *~
