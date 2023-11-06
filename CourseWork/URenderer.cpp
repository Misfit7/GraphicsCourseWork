#include "URenderer.h"

#include <algorithm>

URenderer::URenderer(Window& parent) : OGLRenderer(parent) {
    camera = new Camera(-45.0f, 0.0f, (Vector3(2200.0f, 1000.0f, 1000.0f)));
    light = new Light(Vector3(0.0f, 0.0f, 0.0f), Vector4(1, 1, 1, 1), 1000.0f);
    quad = Mesh::GenerateQuad();

    //shader
    skyboxShader = new Shader("skyboxVertex.glsl", "skyboxFragment.glsl");
    sceneShader = new Shader("SceneVertex.glsl", "SceneFragment.glsl");
    shadowShader = new Shader("shadowVert.glsl", "shadowFrag.glsl");
    if (!sceneShader->LoadSuccess() || !skyboxShader->LoadSuccess() || !shadowShader->LoadSuccess()) {
        return;
    }

    //skybox
    cubeMap = SOIL_load_OGL_cubemap(TEXTUREDIR"Starfield/W.png", TEXTUREDIR"Starfield/E.png",
        TEXTUREDIR"Starfield/U.png", TEXTUREDIR"Starfield/D.png",
        TEXTUREDIR"Starfield/S.png", TEXTUREDIR"Starfield/N.png", SOIL_LOAD_RGB, SOIL_CREATE_NEW_ID, 0);

    //planet
    root = new SceneNode();
    solar = new SolarSystem();
    root->AddChild(solar);

    //glFuntion
    projMatrix = Matrix4::Perspective(1.0f, 10000.0f, (float)width / (float)height, 45.0f);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA); //transparent
    glEnable(GL_TEXTURE_CUBE_MAP_SEAMLESS);
    init = true;
}

URenderer ::~URenderer(void) {
    delete root;
    delete quad;
    delete camera;
    delete skyboxShader;
    delete sceneShader;
    glDeleteTextures(1, &texture);
}

void URenderer::AutoScene() {
    Vector3 earthPosition = solar->getEarthPosiotion();
    earthPosition.y += 1000.0f;
    camera->SetPosition(earthPosition);
    camera->SetYaw(-90.0f);
}

void URenderer::UpdateScene(float dt) {
    camera->UpdateCamera(10 * dt);

    viewMatrix = camera->BuildViewMatrix();
    frameFrustum.FromMatrix(projMatrix * viewMatrix);
    root->Update(dt);
}

void URenderer::BuildNodeLists(SceneNode* from) {
    if (frameFrustum.InsideFrustum(*from)) {
        Vector3 dir = from->GetWorldTransform().GetPositionVector() - camera->GetPosition();
        from->SetCameraDistance(Vector3::Dot(dir, dir));
        if (from->GetColour().w < 1.0f) {
            transparentNodeList.push_back(from);
        }
        else {
            nodeList.push_back(from);
        }
    }
    for (vector <SceneNode*>::const_iterator i = from->GetChildIteratorStart(); i != from->GetChildIteratorEnd(); ++i) {
        BuildNodeLists((*i));
    }
}

void URenderer::SortNodeLists() {
    std::sort(transparentNodeList.rbegin(), transparentNodeList.rend(), SceneNode::CompareByCameraDistance);
    std::sort(nodeList.begin(), nodeList.end(), SceneNode::CompareByCameraDistance);
}

void URenderer::DrawNodes() {
    for (const auto& i : nodeList) {
        DrawNode(i);
    }
    for (const auto& i : transparentNodeList) {
        DrawNode(i);
    }
}

void URenderer::DrawNode(SceneNode* n) {
    if (n->GetMesh()) {
        Matrix4 model = n->GetWorldTransform() * Matrix4::Scale(n->GetModelScale());
        glUniformMatrix4fv(glGetUniformLocation(sceneShader->GetProgram(), "modelMatrix"), 1, false, model.values);
        glUniform4fv(glGetUniformLocation(sceneShader->GetProgram(), "nodeColour"), 1, (float*)&n->GetColour());
        texture = n->GetTexture();
        glActiveTexture(GL_TEXTURE0);
        SetTextureRepeating(texture, true);
        glBindTexture(GL_TEXTURE_2D, texture);
        glUniform1i(glGetUniformLocation(sceneShader->GetProgram(), "useTexture"), texture);
        n->Draw(*this);
    }
}

void URenderer::DrawSkybox() {
    glDepthMask(GL_FALSE);

    BindShader(skyboxShader);
    UpdateShaderMatrices();

    quad->Draw();

    glDepthMask(GL_TRUE);
}

void URenderer::DrawMainScene() {
    BuildNodeLists(root);
    SortNodeLists();
    BindShader(sceneShader);
    UpdateShaderMatrices();
    glUniform1i(glGetUniformLocation(sceneShader->GetProgram(), "diffuseTex"), 0);
    DrawNodes();
    ClearNodeLists();

}

void URenderer::DrawShadowScene()
{

}

void URenderer::RenderScene() {
    glClear(GL_DEPTH_BUFFER_BIT | GL_COLOR_BUFFER_BIT);
    DrawSkybox();
    DrawMainScene();
    DrawShadowScene();
}

void URenderer::ClearNodeLists() {
    transparentNodeList.clear();
    nodeList.clear();
}