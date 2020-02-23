CFLAGS=-O0

bin:
	mkdir -p bin

tri_ubo.vert.spv: bin tri_ubo.vert.glsl
	glslc $(CFLAGS) -fshader-stage=vert tri_ubo.vert.glsl -o bin/tri_ubo.vert.spv

tri_ubo.frag.spv: bin tri_ubo.frag.glsl
	glslc $(CFLAGS) -fshader-stage=frag tri_ubo.frag.glsl -o bin/tri_ubo.frag.spv

attachment_read.vert.spv: bin attachment_read.vert.glsl
	glslc $(CFLAGS) -fshader-stage=vert attachment_read.vert.glsl -o bin/attachment_read.vert.spv

attachment_read.frag.spv: bin attachment_read.frag.glsl
	glslc $(CFLAGS) -fshader-stage=frag attachment_read.frag.glsl -o bin/attachment_read.frag.spv

tri_ubo: tri_ubo.vert.spv tri_ubo.frag.spv

attachment_read: attachment_read.vert.spv attachment_read.frag.spv

main: tri_ubo attachment_read

clean:
	rm -rf bin
	rm -f *~
