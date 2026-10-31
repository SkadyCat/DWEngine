"""Compile the actual production material translations with UE's bundled DXC."""
import concurrent.futures,json,pathlib,subprocess
from shader_translation import shader_code
root=pathlib.Path(__file__).resolve().parents[2]
out=root/'Saved/Verification/UEIntegration/Shaders';out.mkdir(parents=True,exist_ok=True)
dxc=pathlib.Path('G:/UnrealEngine-Angelscript-5.8.3/Engine/Binaries/ThirdParty/ShaderConductor/Win64/dxc.exe')
manifest=json.loads((root/'Saved/WebVFX/Source/manifest.json').read_text(encoding='utf-8'))
def check(data):
 source=out/(data['id']+'.hlsl');output=out/(data['id']+'.dxil')
 code=shader_code(data)
 source.write_text('#define Texture2DSample(T,S,U) T.Sample(S,U)\nTexture2D PortalTexture;SamplerState PortalTextureSampler;\n'
  'cbuffer VerificationInputs {float3 Color;float Opacity;float3 WorldNormal;float Age;float3 LocalPosition;float Fade;float Mode,Clock,Inner,Angle;}\n'
  'float4 Main(float4 Position:SV_Position,float2 UV:TEXCOORD0):SV_Target{\n'+code+'\n}',encoding='utf-8')
 process=subprocess.run([str(dxc),'-E','Main','-T','ps_6_0','-O3','-WX','-Fo',str(output),str(source)],capture_output=True,text=True,encoding='utf-8',errors='replace')
 diagnostics=(process.stdout+process.stderr).strip()
 if process.returncode:(out/(data['id']+'.log')).write_text(diagnostics,encoding='utf-8')
 return {'id':data['id'],'ok':process.returncode==0,'diagnostics':diagnostics if process.returncode else ''}
with concurrent.futures.ThreadPoolExecutor(max_workers=4) as pool:results=list(pool.map(check,manifest['materials']))
proof={'ok':all(r['ok'] for r in results),'materials':len(results),'failed':[r for r in results if not r['ok']]}
(out.parent/'vfx-shaders-check.json').write_text(json.dumps(proof,indent=2),encoding='utf-8')
print(json.dumps(proof,ensure_ascii=False));raise SystemExit(0 if proof['ok'] else 1)
