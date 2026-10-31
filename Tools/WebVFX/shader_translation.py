"""Pure offline translation shared by the UE importer and DXC preflight."""
import re

def hlsl(fragment):
    fragment = re.sub(r'#include\s*<colorspace_fragment>', '', fragment)
    code = re.sub(r'\b(?:uniform|varying)\s+\w+\s+[^;]+;', '', fragment)
    code = re.sub(r'\bvec([234])\b', r'float\1', code)
    code = re.sub(r'\bmat([234])\b', r'float\1x\1', code)
    for a, b in [('fract', 'frac'), ('mix', 'lerp'), ('atan', 'atan2'), ('mod', 'fmod')]:
        code = re.sub(r'\b' + a + r'\s*\(', b + '(', code)
    code = re.sub(r'float([234])\(\s*(-?[\d.]+)\s*\)', lambda m: 'float' + m[1] + '(' + ','.join([m[2]] * int(m[1])) + ')', code)
    code = code.replace('gl_PointCoord', 'vUv').replace('vColor', 'color')
    main = re.search(r'void\s+main\s*\(\s*\)\s*\{', code)
    assert main, 'Unsupported fragment entry point'
    helpers = code[:main.start()]
    body = code[main.end():code.rfind('}')]
    # GLSL permits the local tint initializer to read the outer uniform.
    # HLSL resolves the same spelling to the newly declared, uninitialized local.
    # Give that local its own name while retaining the original uniform input.
    shadow = re.search(r'\b(float[234]?)\s+(\w+)\s*=\s*\2\s*;', body)
    if shadow:
        name = shadow[2]; local = 'local_' + name
        body = (body[:shadow.start()] + shadow[1] + ' ' + local + '=' + name + ';' +
                re.sub(r'\b' + re.escape(name) + r'\b', local, body[shadow.end():]))
    body = re.sub(r'\bdiscard\s*;', 'return float4(0,0,0,0);', body)
    body = body.replace('gl_FragColor', 'result')
    body = re.sub(r'\breturn\s*;', 'return result;', body)
    constants=re.findall(r'\bconst\s+float\s+(\w+)\s*=\s*([^;]+);',helpers)
    helpers=re.sub(r'\bconst\s+float\s+\w+\s*=\s*[^;]+;', '', helpers)
    fields=''.join('float '+name+';' for name,value in constants)
    assignments=''.join('fx.'+name+'='+value+';' for name,value in constants)
    # HLSL custom expressions permit local structs with member functions.
    # The original helpers and arithmetic remain visible in the saved asset.
    return ('struct WebFX { float3 color; float age,fade,mode,clock,inner,angle;\n' + fields + helpers +
            '\nfloat4 run(float2 vUv){float4 result=float4(0,0,0,0);' + body + '\nreturn result;} };\n' +
            'WebFX fx;fx.color=Color;fx.age=Age;fx.fade=Fade;fx.mode=Mode;fx.clock=Clock;fx.inner=Inner;fx.angle=Angle;'+assignments+'return fx.run(UV);')

def shader_code(data):
    fragment=data.get("fragment") or "uniform vec3 color;uniform float opacity;void main(){gl_FragColor=vec4(color,opacity);}"
    code=hlsl(fragment)
    if not data.get("fragment"):
        code=code.replace("float age,fade,mode,clock,inner,angle;", "float age,fade,mode,clock,inner,angle,opacity;").replace("fx.color=Color;", "fx.color=Color;fx.opacity=Opacity;")
    if data.get("impactSolid"):
        code=code.replace("float3 color;", "float3 color,normalHit;").replace("fx.color=Color;", "fx.color=Color;fx.normalHit=WorldNormal;")
    if data.get("portal"):
        variant=data["portal"]["variant"]
        angle="Age*.35" if variant==0 else "sin(Age*6.)*.06" if variant==1 else "-Age*1.3"
        code=("float a="+angle+";float c=cos(a),s=sin(a);float2 p=(UV-.5)*1.12;"
              "float2 uv=float2(c*p.x+s*p.y,-s*p.x+c*p.y)+.5;"
              "if(any(uv<0)||any(uv>1))return float4(0,0,0,0);"
              "float4 art=Texture2DSample(PortalTexture,PortalTextureSampler,uv);"
              "return float4(art.rgb,art.a*saturate(Fade));")
    if data.get("weaponClip"):
        # Imported native Y is negative Three.js Z (the source blade/front axis).
        code="return float4(Color*"+str(data.get('weaponGain',1.0))+",-LocalPosition.y*.01+Inner<0?0:Opacity);"
    if data.get("portal",{}).get("ea"):
        code=('float2 p=UV-.5;float a=Age*.35,c=cos(a),s=sin(a);float2 uv=float2(c*p.x+s*p.y,-s*p.x+c*p.y)+.5;'
              'float4 tex=Texture2DSample(PortalTexture,PortalTextureSampler,uv);float r=length(p)*2.;float outer=exp(-abs(r-.91)*150.);float inner=exp(-abs(r-.69)*130.);'
              'float glyph=pow(max(0.,cos(atan2(p.y,p.x)*24.+Age*2.)),16.)*exp(-abs(r-.8)*90.);float ink=max(tex.r,max(tex.g,tex.b));'
              'float3 color=lerp(float3(1.6,.06,.16),float3(1.6,.9,.2),saturate(inner+outer+glyph));return float4(color*(ink+outer+inner+glyph),max(tex.a,outer+inner+glyph)*Fade);')
    return code
