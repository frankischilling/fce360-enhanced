//--------------------------------------------------------------------------------------
// File: Pick.fx
//
// The effect file for the Pick sample.  
// 
// Copyright (c) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------


//--------------------------------------------------------------------------------------
// Global variables
//--------------------------------------------------------------------------------------
float4 g_MaterialAmbientColor;      // Material's ambient color
float4 g_MaterialDiffuseColor;      // Material's diffuse color
float3 g_LightDir;                  // Light's direction in world space
float4 g_LightDiffuse;              // Light's diffuse color

float    g_fTime;                   // App's time in seconds
float4x4 g_mWorld;                  // World matrix for object
float4x4 g_mWorldViewProjection;    // World * View * Projection matrix

texture g_MeshTexture;              // Color texture for mesh
texture g_bgTexture;              	// Color texture for mesh

float2 g_TexelSize : TEXELSIZE;

float4 vGravity = float4(0,-2.0,0,0);
float Layer     = 0; // 0 to 1 for the level


//--------------------------------------------------------------------------------------
// Texture samplers
//--------------------------------------------------------------------------------------
sampler MeshTextureSampler = 
sampler_state
{
    Texture = <g_MeshTexture>;
	MipFilter = POINT ;
    MinFilter = POINT;
    MagFilter = POINT;
};

sampler BgTextureSampler = 
sampler_state
{
    Texture = <g_bgTexture>;
	MipFilter = LINEAR ;
    MinFilter = LINEAR;
    MagFilter = LINEAR;
};

//--------------------------------------------------------------------------------------
// Vertex shader output structure
//--------------------------------------------------------------------------------------
struct VS_OUTPUT
{
    float4 Position   : POSITION;   // vertex position 
    float4 Diffuse    : COLOR0;     // vertex diffuse color (note that COLOR0 is clamped from 0..1)
    float2 TextureUV  : TEXCOORD0;  // vertex texture coords 
};


//--------------------------------------------------------------------------------------
// This shader computes standard transform and lighting
//--------------------------------------------------------------------------------------
VS_OUTPUT RenderSceneVS( float4 vPos : POSITION, 
                         float3 vNormal : NORMAL,
                         float2 vTexCoord0 : TEXCOORD0 )
{
    VS_OUTPUT Output;
	float4 Pos = vPos;
	
	Pos.z -= cos( Pos.x/2.5);
	Pos.z -= cos( Pos.y/2.5);
	
	Output.Diffuse    = 0;
    Output.Position   = vPos+ mul(Pos,g_mWorldViewProjection);
    Output.TextureUV  = vTexCoord0; 
    
    return Output;    
}


//--------------------------------------------------------------------------------------
// This shader computes standard transform and lighting
//--------------------------------------------------------------------------------------
VS_OUTPUT FullScreenVS( float4 vPos : POSITION, 
                         float3 vNormal : NORMAL,
                         float2 vTexCoord0 : TEXCOORD0 )
{
    VS_OUTPUT Output;
	Output.Diffuse = 0;

	vPos.xy = sign(vPos.xy);

	Output.Position.xy = vPos.xy;
	Output.Position.z = 1.0f;
	Output.Position.w = 1.0f;
	// Image-space  
	Output.TextureUV.x = 0.5f * (1 + vPos.x);  
	Output.TextureUV.y = 0.5f * (1 - vPos.y);  

    return Output;    
}

//--------------------------------------------------------------------------------------
// This shader computes standard transform and lighting
//--------------------------------------------------------------------------------------
VS_OUTPUT TvScreenVS( float4 vPos : POSITION, 
                         float3 vNormal : NORMAL,
                         float2 vTexCoord0 : TEXCOORD0 )
{
    VS_OUTPUT Output;
	
	float mul43 = 4.0f/3.0f;
	
	Output.Diffuse = 0;
	
	vPos.xy = sign(vPos.xy);  
	vPos.x = vPos.x / mul43;

	Output.Position = float4(vPos.xy, 0, 1);  
	Output.Position.z = 0.0f;
	// Image-space  
	Output.TextureUV.x = 0.5f * (1 + sign(vPos.x));  
	Output.TextureUV.y = 0.5f * (1 - vPos.y);  
	
    return Output;    
}

//--------------------------------------------------------------------------------------
// Pixel shader output structure
//--------------------------------------------------------------------------------------
struct PS_OUTPUT
{
    float4 RGBColor : COLOR0;  // Pixel color    
};

//--------------------------------------------------------------------------------------
// This shader outputs the pixel's color by modulating the texture's
// color with diffuse material color
//--------------------------------------------------------------------------------------
PS_OUTPUT RenderScenePS( VS_OUTPUT In ) 
{ 
    PS_OUTPUT Output;

	Output.RGBColor = tex2D(MeshTextureSampler, In.TextureUV);
	Output.RGBColor.a = 1.0f;
    return Output;
}
PS_OUTPUT RenderBgPS( VS_OUTPUT In ) 
{ 
    PS_OUTPUT Output;

	Output.RGBColor = tex2D(BgTextureSampler, In.TextureUV);
	Output.RGBColor.a = 1.0f;
    return Output;
}

//--------------------------------------------------------------------------------------
// Renders Background 
//--------------------------------------------------------------------------------------
technique RenderFullScreen
{
    pass P0
    {          
		VertexShader = compile vs_2_0 FullScreenVS();
        PixelShader  = compile ps_2_0 RenderBgPS(); 
    }
}

technique RenderModel
{
    pass P0
    {          
		VertexShader = compile vs_2_0 RenderSceneVS();
        PixelShader  = compile ps_2_0 RenderScenePS(); 
    }
}

//--------------------------------------------------------------------------------------
// Renders scene 
//--------------------------------------------------------------------------------------
technique RenderModelTv
{
    pass P0
    {          
		VertexShader = compile vs_2_0 TvScreenVS();
        PixelShader  = compile ps_2_0 RenderScenePS(); 
    }
}
//--------------------------------------------------------------------------------------
// Renders scene 
//--------------------------------------------------------------------------------------
technique RenderModelFullScreen
{
    pass P0
    {          
		VertexShader = compile vs_2_0 FullScreenVS();
        PixelShader  = compile ps_2_0 RenderScenePS(); 
    }
}