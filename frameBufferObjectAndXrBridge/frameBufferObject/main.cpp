/**
 * @file		main.cpp
 * @brief	Frame Buffer Object (FBO) example with XrBridge and stereoscopic rendering
 *
 * @author	Achille Peternier (C) SUPSI [achille.peternier@supsi.ch]
 * @author	Lorenzo Adam Piazza (C) SUPSI [lorenzoadam.piazza@student.supsi.ch]
 */



//////////////
// #INCLUDE //
//////////////

   // GLM:   
   #define GLM_ENABLE_EXPERIMENTAL
   #include <glm/glm.hpp>
   #include <glm/gtc/matrix_transform.hpp>
   #include <glm/gtc/matrix_inverse.hpp>
   #include <glm/gtc/type_ptr.hpp>
   #include <glm/gtx/string_cast.hpp>

   // GLEW:
   #include <GL/glew.h>

   // FreeGLUT:
   #include <GL/freeglut.h>

   // FreeImage:
   #include <FreeImage.h>

   // C/C++:
   #include <iostream>
   #include <string>
   #include <stdio.h>

   // Shader class:
   #include "shader.h"

   // Fbo class:
   #include "fbo.h"

   // XrBridge:
   #include "xrbridge.hpp"
   


/////////////
// #DEFINE //
/////////////

   // Window size:
   #define APP_WINDOWSIZEX   1024
   #define APP_WINDOWSIZEY   512
   
   // Additional log:
   #define APP_VERBOSE



/////////////
// GLOBALS //
/////////////
  
   // Context information:
   int windowId;
   bool wireframe = false;

   // Light pos:
   glm::vec3 lightPos = glm::vec3(0.0f, 1.0f, -10.0f);

   // Matrices:
   glm::mat4 ortho;

   // Textures:
   unsigned int texId = 0;

   // Vertex buffers:
   unsigned int globalVao = 0;
   unsigned int vertexVbo = 0;
   unsigned int normalVbo = 0;
   unsigned int texCoordVbo = 0;
   unsigned int boxVertexVbo = 0;
   unsigned int boxTexCoordVbo = 0;

   // PPL shader:
   Shader *pplVs = nullptr;
   Shader *pplFs = nullptr;
   Shader *pplShader = nullptr;
   int projLoc = -1;
   int mvLoc = -1;
   int normalMatLoc = -1;
   int matAmbientLoc = -1;
   int matDiffuseLoc = -1;
   int matSpecularLoc = -1;
   int matShininessLoc = -1;
   int lightPositionLoc = -1;
   int lightAmbientLoc = -1;
   int lightDiffuseLoc = -1;
   int lightSpecularLoc = -1;

   // Passthrough shader:
   Shader *passthroughVs = nullptr;
   Shader *passthroughFs = nullptr;
   Shader *passthroughShader = nullptr;
   int ptProjLoc = -1;
   int ptMvLoc = -1;
   int ptColorLoc = -1;

   // XrBridge interface:
   XrBridge *xrbridge = nullptr;



/////////////
// SHADERS //
/////////////

// Blinn-Phong PPL with texture mapping:
const char *pplVertShader = R"(
   #version 440 core

   // Uniforms:
   uniform mat4 projection;
   uniform mat4 modelview;
   uniform mat3 normalMatrix;

   // Attributes:
   layout(location = 0) in vec3 in_Position;
   layout(location = 1) in vec3 in_Normal;
   layout(location = 2) in vec2 in_TexCoord;

   // Varying:
   out vec4 fragPosition;
   out vec3 normal;
   out vec2 texCoord;

   void main(void)
   {
      fragPosition = modelview * vec4(in_Position, 1.0f);
      gl_Position = projection * fragPosition;
      normal = normalMatrix * in_Normal;
      texCoord = in_TexCoord;
   }
)";

const char *pplFragShader = R"(
   #version 440 core

   in vec4 fragPosition;
   in vec3 normal;
   in vec2 texCoord;
   
   out vec4 fragOutput;

   // Material properties:
   uniform vec3 matAmbient;
   uniform vec3 matDiffuse;
   uniform vec3 matSpecular;
   uniform float matShininess;

   // Light properties:
   uniform vec3 lightPosition;
   uniform vec3 lightAmbient;
   uniform vec3 lightDiffuse;
   uniform vec3 lightSpecular;

   // Texture mapping:
   layout(binding = 0) uniform sampler2D texSampler;

   void main(void)
   {
      // Texture element:
      vec4 texel = texture(texSampler, texCoord);

      // Ambient term:
      vec3 fragColor = matAmbient * lightAmbient;

      // Diffuse term:
      vec3 _normal = normalize(normal);
      vec3 lightDirection = normalize(lightPosition - fragPosition.xyz);
      float nDotL = dot(lightDirection, _normal);
      if (nDotL > 0.0f)
      {
         fragColor += matDiffuse * nDotL * lightDiffuse;

         // Specular term:
         vec3 halfVector = normalize(lightDirection + normalize(-fragPosition.xyz));
         float nDotHV = dot(_normal, halfVector);
         fragColor += matSpecular * pow(nDotHV, matShininess) * lightSpecular;
      }
      
      // Final color:
      fragOutput = texel * vec4(fragColor, 1.0f);

      // Gamma correction (RGB -> sRGB):
      fragOutput.r = pow(fragOutput.r, 1.0f / 2.2f);
      fragOutput.g = pow(fragOutput.g, 1.0f / 2.2f);
      fragOutput.b = pow(fragOutput.b, 1.0f / 2.2f);
   }
)";

// Passthrough shader with texture mapping:
const char *passthroughVertShader = R"(
   #version 440 core

   // Uniforms:
   uniform mat4 projection;
   uniform mat4 modelview;

   // Attributes:
   layout(location = 0) in vec2 in_Position;
   layout(location = 2) in vec2 in_TexCoord;

   // Varying:
   out vec2 texCoord;

   void main(void)
   {
      gl_Position = projection * modelview * vec4(in_Position, 0.0f, 1.0f);
      texCoord = in_TexCoord;
   }
)";

const char *passthroughFragShader = R"(
   #version 440 core
   
   in vec2 texCoord;
   
   uniform vec4 color;

   out vec4 fragOutput;

   // Texture mapping:
   layout(binding = 0) uniform sampler2D texSampler;

   void main(void)
   {
      // Texture element:
      vec4 texel = texture(texSampler, texCoord);
      
      // Final color:
      fragOutput = color * texel;
   }
)";



/////////////
// METHODS //
/////////////

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/**
 * Load image from file.
 */
void buildTexture()
{
   const char texFilename[] = "bricks.jpg";

   // Load texture:
   FIBITMAP *fBitmap = FreeImage_Load(FreeImage_GetFileType(texFilename, 0), texFilename);
   if (fBitmap == nullptr)
      std::cout << "[ERROR] Unable to load texture" << std::endl;
   int intFormat = GL_RGB;
   GLenum extFormat = GL_BGR;
   if (FreeImage_GetBPP(fBitmap) == 32)
   {
      intFormat = GL_RGBA;
      extFormat = GL_BGRA;
   }

   // Update texture content:
   glBindTexture(GL_TEXTURE_2D, texId);
   glTexImage2D(GL_TEXTURE_2D, 0, intFormat, FreeImage_GetWidth(fBitmap), FreeImage_GetHeight(fBitmap), 0, extFormat, GL_UNSIGNED_BYTE, (void *) FreeImage_GetBits(fBitmap));
   glGenerateMipmap(GL_TEXTURE_2D); // <-- we can use this now!

   // Set circular coordinates:
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

   // Set min/mag filters:
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);

   // Free resources:
   FreeImage_Unload(fBitmap);
}



///////////////
// CALLBACKS //
///////////////

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/**
 * Debug message callback for OpenGL. See https://www.opengl.org/wiki/Debug_Output
 */
void __stdcall DebugCallback(GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length, const GLchar* message, GLvoid* userParam)
{
   std::cout << "OpenGL says: \"" << std::string(message) << "\"" << std::endl;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/**
 * This is the main rendering routine automatically invoked by FreeGLUT.
 */
void displayCallback()
{
   // Clear the context back buffer:
   Fbo::disable();
   glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
   glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

   // Store the current viewport size:
   GLint prevViewport[4];
   glGetIntegerv(GL_VIEWPORT, prevViewport);

   // Update:
   xrbridge->update();

   xrbridge->render([&] (
      const XrBridge::Eye eye,
      std::shared_ptr<Fbo> fbo,
      const glm::mat4 projection_matrix,
      const glm::mat4 view_matrix,
      const uint32_t width,
      const uint32_t height) {

#ifdef APP_VERBOSE
      const int c = eye == XrBridge::Eye::LEFT ? 0 : 1;
      std::cout << "Eye " << c << " proj matrix: " << glm::to_string(projection_matrix) << std::endl;
      std::cout << "Eye " << c << " modelview matrix: " << glm::to_string(view_matrix) << std::endl;
#endif

      const glm::mat4 inverse_view_matrix = glm::inverse(view_matrix);
      
      // Render into this FBO:
      fbo->render();

      // Clear the FBO content:
      glClearColor(0.53f, 0.8f, 0.9f, 1.0f); // Sky blue
      glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

      ////////////////
      // 3D rendering:

      // Setup params for the PPL shader:
      pplShader->render();
      pplShader->setMatrix(projLoc, projection_matrix);
      pplShader->setMatrix(mvLoc, inverse_view_matrix);
      glm::mat3 normalMatrix = glm::inverseTranspose(glm::mat3(inverse_view_matrix));
      pplShader->setMatrix3(normalMatLoc, normalMatrix);

      // Set light pos:
      pplShader->setVec3(lightPositionLoc, glm::vec3(inverse_view_matrix * glm::vec4(lightPos, 1.0f))); // Light position in eye coordinates!

      // Render the plane using its VBOs:
      glBindBuffer(GL_ARRAY_BUFFER, vertexVbo);
      glVertexAttribPointer((GLuint) 0, 3, GL_FLOAT, GL_FALSE, 0, nullptr);
      glEnableVertexAttribArray(0);

      glBindBuffer(GL_ARRAY_BUFFER, normalVbo);
      glVertexAttribPointer((GLuint) 1, 3, GL_FLOAT, GL_FALSE, 0, nullptr);
      glEnableVertexAttribArray(1);

      glBindBuffer(GL_ARRAY_BUFFER, texCoordVbo);
      glVertexAttribPointer((GLuint) 2, 2, GL_FLOAT, GL_FALSE, 0, nullptr);
      glEnableVertexAttribArray(2);

      // Bind the brick texture and render:
      glBindTexture(GL_TEXTURE_2D, texId);
      glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

      ////////////////
      // 2D rendering:

      // Done with the FBO, go back to rendering into the window context buffers:
      Fbo::disable();
      glViewport(0, 0, prevViewport[2], prevViewport[3]);

      // Setup the passthrough shader:
      passthroughShader->render();
      passthroughShader->setMatrix(ptProjLoc, ortho);

      glBindBuffer(GL_ARRAY_BUFFER, boxVertexVbo);
      glVertexAttribPointer((GLuint) 0, 2, GL_FLOAT, GL_FALSE, 0, nullptr);
      glEnableVertexAttribArray(0);

      glDisableVertexAttribArray(1); // We don't need normals for the 2D quad

      glBindBuffer(GL_ARRAY_BUFFER, boxTexCoordVbo);
      glVertexAttribPointer((GLuint) 2, 2, GL_FLOAT, GL_FALSE, 0, nullptr);
      glEnableVertexAttribArray(2);

      if (eye == XrBridge::Eye::LEFT)
      {
         //glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
         //glBlitFramebuffer(0, 0, width, height, 0, 0, APP_WINDOWSIZEX / 2.0f, APP_WINDOWSIZEY, GL_COLOR_BUFFER_BIT, GL_NEAREST);

         // Set a matrix for the left "eye":
         const glm::mat4 f = glm::mat4(1.0f);
         passthroughShader->setMatrix(ptMvLoc, f);
         passthroughShader->setVec4(ptColorLoc, glm::vec4(1.0f, 0.0f, 0.0f, 0.0f));
      }
      else if (eye == XrBridge::Eye::RIGHT)
      {
         //glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
         //glBlitFramebuffer(0, 0, width, height, APP_WINDOWSIZEX / 2.0f, 0, APP_WINDOWSIZEX, APP_WINDOWSIZEY, GL_COLOR_BUFFER_BIT, GL_NEAREST);

         // Set a matrix for the right "eye":
         const glm::mat4 f = glm::translate(glm::mat4(1.0f), glm::vec3(APP_WINDOWSIZEX / 2.0f, 0.0f, 0.0f));
         passthroughShader->setMatrix(ptMvLoc, f);
         passthroughShader->setVec4(ptColorLoc, glm::vec4(0.0f, 1.0f, 1.0f, 0.0f));
      }

      // Bind the FBO buffer as texture and render:
      glBindTexture(GL_TEXTURE_2D, fbo->getTexture(0));
      glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
   });

   Fbo::disable();
   glViewport(0, 0, prevViewport[2], prevViewport[3]);

   // Swap buffers:
   glutSwapBuffers();
   
   // Force rendering refresh:
   glutPostWindowRedisplay(windowId);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/**
 * This callback is invoked each time the window gets resized (and once also when created).
 * @param width new window width
 * @param height new window height
 */
void reshapeCallback(int width, int height)
{
   // ... ignore the params, we want a fixed-size window

   // Update matrices:
   ortho = glm::ortho(0.0f, (float) APP_WINDOWSIZEX, 0.0f, (float) APP_WINDOWSIZEY, -1.0f, 1.0f);

   // (bad) trick to avoid window resizing:
   if (width != APP_WINDOWSIZEX || height != APP_WINDOWSIZEY)
      glutReshapeWindow(APP_WINDOWSIZEX, APP_WINDOWSIZEY);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/**
 * This callback is invoked each time a special keyboard key is pressed.
 * @param key key pressed id
 * @param mouseX mouse X coordinate
 * @param mouseY mouse Y coordinate
 */
void specialCallback(int key, int mouseX, int mouseY)
{
   // Change box rotation:
   const float speed = 1.0f;
   switch (key)
   {
      case GLUT_KEY_UP:
         lightPos.z -= speed;
         break;

      case GLUT_KEY_DOWN:
         lightPos.z += speed;
         break;

      case GLUT_KEY_LEFT:
         lightPos.x -= speed;
         break;

      case GLUT_KEY_RIGHT:
         lightPos.x += speed;
         break;

      case GLUT_KEY_PAGE_UP:
         lightPos.y += speed;
         break;

      case GLUT_KEY_PAGE_DOWN:
         lightPos.y -= speed;
         break;
   }

   // Force rendering refresh:
   glutPostWindowRedisplay(windowId);
}


/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/**
 * This callback is invoked when the mainloop is left and before the context is released.
 */
void closeCallback()
{
   // Free XrBridge:
   xrbridge->free();
   delete xrbridge;

   // Release OpenGL resources while the context is still available:
   glDeleteBuffers(1, &vertexVbo);
   glDeleteBuffers(1, &normalVbo);
   glDeleteBuffers(1, &texCoordVbo);
   glDeleteVertexArrays(1, &globalVao);
   
   glDeleteTextures(1, &texId);
   
   delete pplShader;
   delete pplFs;
   delete pplVs;
   delete passthroughShader;
   delete passthroughFs;
   delete passthroughVs;
}



/////////////
// METHODS //
/////////////

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/**
 * Initialize the shaders used by this program.
 * @return TF
 */
bool buildShaders()
{
   ////////////////////
   // Build PPL shader:
   pplVs = new Shader();
   pplVs->loadFromMemory(Shader::TYPE_VERTEX, pplVertShader);

   pplFs = new Shader();
   pplFs->loadFromMemory(Shader::TYPE_FRAGMENT, pplFragShader);
   
   pplShader = new Shader();
   pplShader->build(pplVs, pplFs);
   pplShader->render();
   
   // Bind params:
   pplShader->bind(0, "in_Position");
   pplShader->bind(1, "in_Normal");
   pplShader->bind(2, "in_TexCoord");

   projLoc = pplShader->getParamLocation("projection");
   mvLoc = pplShader->getParamLocation("modelview");
   normalMatLoc = pplShader->getParamLocation("normalMatrix");

   matAmbientLoc = pplShader->getParamLocation("matAmbient");
   matDiffuseLoc = pplShader->getParamLocation("matDiffuse");
   matSpecularLoc = pplShader->getParamLocation("matSpecular");
   matShininessLoc = pplShader->getParamLocation("matShininess");

   lightPositionLoc = pplShader->getParamLocation("lightPosition");
   lightAmbientLoc = pplShader->getParamLocation("lightAmbient");
   lightDiffuseLoc = pplShader->getParamLocation("lightDiffuse");
   lightSpecularLoc = pplShader->getParamLocation("lightSpecular");
     
   
   ////////////////////////////
   // Build passthrough shader:
   passthroughVs = new Shader();
   passthroughVs->loadFromMemory(Shader::TYPE_VERTEX, passthroughVertShader);

   passthroughFs = new Shader();
   passthroughFs->loadFromMemory(Shader::TYPE_FRAGMENT, passthroughFragShader);
   
   passthroughShader = new Shader();
   passthroughShader->build(passthroughVs, passthroughFs);
   passthroughShader->render();

   // Bind params:
   passthroughShader->bind(0, "in_Position");
   passthroughShader->bind(2, "in_TexCoord");

   ptProjLoc = passthroughShader->getParamLocation("projection");
   ptMvLoc = passthroughShader->getParamLocation("modelview");
   ptColorLoc = passthroughShader->getParamLocation("color");

   // Done:
   return true;
}



//////////
// MAIN //
//////////

/**
 * Application entry point.
 * @param argc number of command-line arguments passed
 * @param argv array containing up to argc passed arguments
 * @return error code (0 on success, error code otherwise)
 */
int main(int argc, char *argv[])
{
   // Credits:
   std::cout << "OpenGL 4.4 Frame Buffer Object (FBO) and XrBridge, A. Peternier (C) SUPSI" << std::endl;
   std::cout << "OpenGL 4.4 Frame Buffer Object (FBO) and XrBridge, Lorenzo Adam Piazza (C)" << std::endl;
   std::cout << std::endl;

   // Init GLUT and set some context flags:
   glutInit(&argc, argv);
   glutInitDisplayMode(GLUT_RGBA | GLUT_DOUBLE | GLUT_DEPTH);
   glutInitContextVersion(4, 4);
   glutInitContextProfile(GLUT_CORE_PROFILE);
   glutInitContextFlags(GLUT_DEBUG);

   // Create window:
   glutInitWindowPosition(100, 100);
   glutInitWindowSize(APP_WINDOWSIZEX, APP_WINDOWSIZEY);
   glutSetOption(GLUT_ACTION_ON_WINDOW_CLOSE, GLUT_ACTION_GLUTMAINLOOP_RETURNS);
   windowId = glutCreateWindow("Frame Buffer Object (FBO) and XrBridge example");

   // Init all available OpenGL extensions:
   GLenum error = glewInit();
   if (GLEW_OK != error)
   {
      std::cout << "[ERROR] " << glewGetErrorString(error) << std::endl;
      return -1;
   }
   else
   {
      if (GLEW_VERSION_4_4)     
      {
         std::cout << "Driver supports OpenGL 4.4\n" << std::endl;
      }
      else
      {
         std::cout << "[ERROR] OpenGL 4.4 not supported\n" << std::endl;
         return -1;
      }
   }

   // Init FreeImage:
   FreeImage_Initialise();

   // Register OpenGL debug callback:
   glDebugMessageCallback((GLDEBUGPROC) DebugCallback, nullptr);
   glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);

   // Init XrBridge:
   xrbridge = new XrBridge();
   if (xrbridge->init("Frame Buffer Object (FBO) and XrBridge example") == false)
   {
      std::cout << "[ERROR] Unable to init XrBridge" << std::endl;
      delete xrbridge;
      return -2;
   }

   xrbridge->set_clipping_planes(1.0f, 1'024.0f);

   // Set callback functions:
   glutDisplayFunc(displayCallback);
   glutReshapeFunc(reshapeCallback);
   glutSpecialFunc(specialCallback);
   glutCloseFunc(closeCallback);
   
   // Init VAO:
   glGenVertexArrays(1, &globalVao);
   glBindVertexArray(globalVao);

   // Compile shaders:
   buildShaders();
   pplShader->render();

   // Set initial material and light params:
   pplShader->setVec3(matAmbientLoc, glm::vec3(0.2f, 0.2f, 0.2f));
   pplShader->setVec3(matDiffuseLoc, glm::vec3(0.7f, 0.7f, 0.7f));
   pplShader->setVec3(matSpecularLoc, glm::vec3(0.6f, 0.6f, 0.6f));
   pplShader->setFloat(matShininessLoc, 128.0f);
   
   pplShader->setVec3(lightAmbientLoc, glm::vec3(1.0f, 1.0f, 1.0f));
   pplShader->setVec3(lightDiffuseLoc, glm::vec3(1.0f, 1.0f, 1.0f));
   pplShader->setVec3(lightSpecularLoc, glm::vec3(1.0f, 1.0f, 1.0f));
   
   // Create a triangle plane with its normals and tex coords:
   float size = 128.0f;
   glm::vec3 *plane = new glm::vec3[4];
   plane[0] = glm::vec3(-size, 0.0f, -size);
   plane[1] = glm::vec3(-size, 0.0f,  size);
   plane[2] = glm::vec3( size, 0.0f, -size);
   plane[3] = glm::vec3( size, 0.0f,  size);

   glm::vec3 *normal = new glm::vec3[4];
   for (int c = 0; c < 4; c++)
      normal[c] = glm::vec3(0.0f, 1.0f, 0.0f);

   glm::vec2 *texCoord = new glm::vec2[4];
   texCoord[0] = glm::vec2(0.0f, 0.0f);
   texCoord[1] = glm::vec2(0.0f, 50.0f);
   texCoord[2] = glm::vec2(50.0f, 0.0f);
   texCoord[3] = glm::vec2(50.0f, 50.0f);

   // Copy data into VBOs:
   glGenBuffers(1, &vertexVbo);
	glBindBuffer(GL_ARRAY_BUFFER, vertexVbo);
   glBufferData(GL_ARRAY_BUFFER, 4 * sizeof(glm::vec3), plane, GL_STATIC_DRAW);
   glVertexAttribPointer((GLuint) 0, 3, GL_FLOAT, GL_FALSE, 0, nullptr);
   glEnableVertexAttribArray(0);
   delete [] plane;
   
   glGenBuffers(1, &normalVbo);
	glBindBuffer(GL_ARRAY_BUFFER, normalVbo);
	glBufferData(GL_ARRAY_BUFFER, 4 * sizeof(glm::vec3), normal, GL_STATIC_DRAW);
   glVertexAttribPointer((GLuint) 1, 3, GL_FLOAT, GL_FALSE, 0, nullptr);
   glEnableVertexAttribArray(1);
   delete [] normal;

   glGenBuffers(1, &texCoordVbo);
	glBindBuffer(GL_ARRAY_BUFFER, texCoordVbo);
	glBufferData(GL_ARRAY_BUFFER, 4 * sizeof(glm::vec2), texCoord, GL_STATIC_DRAW);
   glVertexAttribPointer((GLuint) 2, 2, GL_FLOAT, GL_FALSE, 0, nullptr);
   glEnableVertexAttribArray(2);
   
   // Create a 2D box for screen rendering:
   glm::vec2 *boxPlane = new glm::vec2[4];
   boxPlane[0] = glm::vec2(0.0f, 0.0f);
   boxPlane[1] = glm::vec2(APP_WINDOWSIZEX / 2.0f, 0.0f);
   boxPlane[2] = glm::vec2(0.0f, APP_WINDOWSIZEY);
   boxPlane[3] = glm::vec2(APP_WINDOWSIZEX / 2.0f, APP_WINDOWSIZEY);

   // Copy data into VBOs:
   glGenBuffers(1, &boxVertexVbo);
	glBindBuffer(GL_ARRAY_BUFFER, boxVertexVbo);
   glBufferData(GL_ARRAY_BUFFER, 4 * sizeof(glm::vec2), boxPlane, GL_STATIC_DRAW);
   glVertexAttribPointer((GLuint) 0, 2, GL_FLOAT, GL_FALSE, 0, nullptr);
   delete [] boxPlane;

   texCoord[0] = glm::vec2(0.0f, 0.0f);
   texCoord[1] = glm::vec2(1.0f, 0.0f);
   texCoord[2] = glm::vec2(0.0f, 1.0f);
   texCoord[3] = glm::vec2(1.0f, 1.0f);
   glGenBuffers(1, &boxTexCoordVbo);
	glBindBuffer(GL_ARRAY_BUFFER, boxTexCoordVbo);
	glBufferData(GL_ARRAY_BUFFER, 4 * sizeof(glm::vec2), texCoord, GL_STATIC_DRAW);
   glVertexAttribPointer((GLuint) 2, 2, GL_FLOAT, GL_FALSE, 0, nullptr);
   glEnableVertexAttribArray(2);
   delete [] texCoord;

   // OpenGL settings:
   glClearColor(1.0f, 1.0f, 1.0f, 1.0f);
   glEnable(GL_DEPTH_TEST);

   // Load FBO and its texture:
   GLint prevViewport[4];
   glGetIntegerv(GL_VIEWPORT, prevViewport);

   Fbo::disable();
   glViewport(0, 0, prevViewport[2], prevViewport[3]);
   glClearColor(1.0f, 1.0f, 1.0f, 1.0f);

   // Load texture:
   glGenTextures(1, &texId);   
   buildTexture();
   glBindTexture(GL_TEXTURE_2D, texId);    
   
   // Enter the main FreeGLUT processing loop:
   glutMainLoop();
   
   // Release FreeImage:
   FreeImage_DeInitialise();
   
   // Done:
   std::cout << "[application terminated]" << std::endl;
   return 0;
}
