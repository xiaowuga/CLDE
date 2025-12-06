#version 320 es
precision mediump float;
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec2 aTexCoords;
layout(location = 7) in vec4 instanceMatrix0; // 第一列
layout(location = 8) in vec4 instanceMatrix1; // 第二列
layout(location = 9) in vec4 instanceMatrix2; // 第三列
layout(location = 10) in vec4 instanceMatrix3; // 第四列


out vec2 TexCoords;
out vec3 FragPos;
out vec3 Normal;


uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;
uniform bool invertedNormals;
uniform bool isNotInstanced; // 是否不使用实例化渲染
void main()
{

    mat4 modelMatrix = mat4(instanceMatrix0, instanceMatrix1, instanceMatrix2, instanceMatrix3);
    mat4 finalModel = !isNotInstanced? model * modelMatrix : model; // 合并变换

    vec4 viewPos = view * finalModel * vec4(aPos, 1.0);
    FragPos = viewPos.xyz;
    TexCoords = aTexCoords;

    mat4 viewMatrix = view * finalModel;
    mat3 normalMatrix = transpose(inverse(mat3(viewMatrix)));
    Normal = normalMatrix * (invertedNormals ? -aNormal : aNormal);

    //这个是对的法线，需要所有单个小模型一起做个变换
    //mat3 normalMatrix1 = transpose(inverse(mat3(finalModel)));
    //Normal = normalMatrix1 * (invertedNormals ? -aNormal : aNormal);
//vec3 WorldPos = vec3(finalModel * vec4(aPos , 1.0));
//gl_Position =  projection * view * vec4(WorldPos, 1.0);

    gl_Position = projection * viewPos;

}


/*
out vec2 TexCoords;
out vec3 FragPos;
out vec3 Normal;
//shadow mapping use
//out vec4 FragPosLightSpace;

uniform mat4 projection;
uniform mat4 view;
uniform mat4 model;
//uniform mat3 normalMatrix;
//shadow mapping use
//uniform mat4 lightSpaceMatrix;

uniform bool isNotInstanced; // 是否不使用实例化渲染
void main()
{

    mat4 modelMatrix = mat4(instanceMatrix0, instanceMatrix1, instanceMatrix2, instanceMatrix3);
    mat4 finalModel = !isNotInstanced? model * modelMatrix : model; // 合并变换
    TexCoords = aTexCoords;
    vec3 WorldPos = vec3(finalModel * vec4(aPos , 1.0));

    //view 空间法线，需要所有单个小模型一起做个变换
    mat3 normalMatrix1 = transpose(inverse(mat3(view * finalModel)));

    Normal = normalMatrix1 * aNormal;

    gl_Position =  projection * view * vec4(WorldPos, 1.0);
}
*/