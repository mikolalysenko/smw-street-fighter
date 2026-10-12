export async function createPresenter(canvas, onStatus) {
  if (navigator.gpu) {
    try {
      const adapter = await navigator.gpu.requestAdapter({powerPreference:'high-performance'});
      if (!adapter) throw new Error('No WebGPU adapter');
      const device = await adapter.requestDevice();
      const context = canvas.getContext('webgpu');
      if (!context) throw new Error('WebGPU canvas unavailable');
      const format = navigator.gpu.getPreferredCanvasFormat();
      context.configure({device,format,alphaMode:'opaque'});
      const texture = device.createTexture({size:[256,224],format:'rgba8unorm',
        usage:GPUTextureUsage.TEXTURE_BINDING|GPUTextureUsage.COPY_DST});
      const shader = device.createShaderModule({code:`
        struct Vertex { @builtin(position) position:vec4f, @location(0) uv:vec2f }
        @vertex fn vs(@builtin(vertex_index) i:u32)->Vertex {
          let p=array<vec2f,3>(vec2f(-1,-1),vec2f(3,-1),vec2f(-1,3));
          var v:Vertex; v.position=vec4f(p[i],0,1);
          v.uv=vec2f((p[i].x+1)*0.5,(1-p[i].y)*0.5); return v;
        }
        @group(0) @binding(0) var frame:texture_2d<f32>;
        @group(0) @binding(1) var nearest:sampler;
        @fragment fn fs(v:Vertex)->@location(0) vec4f {
          let c=textureSample(frame,nearest,v.uv);return vec4f(c.b,c.g,c.r,1);
        }`});
      const pipeline = device.createRenderPipeline({layout:'auto',
        vertex:{module:shader,entryPoint:'vs'},fragment:{module:shader,entryPoint:'fs',targets:[{format}]},
        primitive:{topology:'triangle-list'}});
      const bindings = device.createBindGroup({layout:pipeline.getBindGroupLayout(0),entries:[
        {binding:0,resource:texture.createView()},
        {binding:1,resource:device.createSampler({magFilter:'nearest',minFilter:'nearest'})},
      ]});
      let failure = null,fallback=null;
      device.addEventListener('uncapturederror', event => { failure = event.error.message; });
      device.lost.then(info => { failure = info.message || 'WebGPU device lost'; });
      onStatus('WebGPU');
      return {
        get kind(){return fallback?'Canvas fallback':'WebGPU';},
        draw(bytes) {
          if(failure && !fallback) {
            console.warn('WebGPU interrupted; switching to pixel canvas.',failure);
            const replacement=canvas.cloneNode();canvas.replaceWith(replacement);canvas=replacement;
            fallback=canvasPresenter(canvas,onStatus);
          }
          if(fallback){fallback.draw(bytes);return;}
          device.queue.writeTexture({texture},bytes,{bytesPerRow:1024,rowsPerImage:224},[256,224]);
          const encoder=device.createCommandEncoder();
          const pass=encoder.beginRenderPass({colorAttachments:[{
            view:context.getCurrentTexture().createView(),loadOp:'clear',storeOp:'store',
            clearValue:{r:0,g:0,b:0,a:1},
          }]});
          pass.setPipeline(pipeline);pass.setBindGroup(0,bindings);pass.draw(3);pass.end();
          device.queue.submit([encoder.finish()]);
        },
      };
    } catch (error) {
      console.warn('WebGPU unavailable; using pixel canvas.',error);
      // A canvas which acquired a WebGPU context cannot acquire a 2D one.
      const replacement=canvas.cloneNode();canvas.replaceWith(replacement);canvas=replacement;
    }
  }
  return canvasPresenter(canvas,onStatus);
}
function canvasPresenter(canvas,onStatus) {
  const context=canvas.getContext('2d',{alpha:false});
  const bitmap=new ImageData(256,224);
  onStatus('Canvas fallback');
  return {kind:'Canvas fallback',draw(bytes) {
    for(let i=0;i<bytes.length;i+=4) {
      bitmap.data[i]=bytes[i+2];bitmap.data[i+1]=bytes[i+1];
      bitmap.data[i+2]=bytes[i];bitmap.data[i+3]=255;
    }
    context.putImageData(bitmap,0,0);
  }};
}
