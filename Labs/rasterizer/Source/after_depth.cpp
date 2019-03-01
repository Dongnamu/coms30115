#include <iostream>
#include <glm/glm.hpp>
#include <SDL.h>
#include "SDLauxiliary.h"
#include "TestModelH.h"
#include <stdint.h>
#include <limits.h>

using namespace std;
using glm::vec3;
using glm::mat3;
using glm::vec4;
using glm::vec2;
using glm::mat4;
using glm::ivec2;


#define SCREEN_WIDTH 1080
#define SCREEN_HEIGHT 1080
#define FULLSCREEN_MODE false
#define PI 3.14159
#define ROWS 500

float maxFloat = std::numeric_limits<float>::max();

float yaw = 2 * PI / 180;

float focal_length = SCREEN_HEIGHT / 2;


struct Camera{
  vec4 position;
  mat4 basis;
  vec3 center;
};

struct Pixel {
    int x;
    int y;
    float zinv;
};

Camera camera = {
  .position = vec4(0,0,-3.001, 1.0),
  .basis = mat4(vec4(1,0,0,0), vec4(0,1,0,0), vec4(0,0,1,0), vec4(0,0,0,1)),
  // .center = vec3(0.003724, 0.929729, 0.07459)
  .center = vec3(0,0,0)
};



// vec4 cameraPos(0, 0, -2, 1.0);
mat4 R;
bool escape = false;
bool is_lookAt = false;


/* ----------------------------------------------------------------------------*/
/* FUNCTIONS                                                                   */

void Update();
void Draw(screen* screen, const vector<Triangle>& triangles);
void VertexShader(const vec4& v, Pixel& p);
void DrawLineSDL(screen* surface, ivec2 a, ivec2 b, vec3 color);
void DrawPolygonEdges(screen* screen, const vector<vec4>& vertices);
void ComputePolygonRows(const vector<Pixel>& vertexPixels, vector<Pixel>& leftPixels, vector<Pixel>& rightPixels);
void Interpolate(Pixel a, Pixel b, vector<Pixel>& result);
void DrawRows( screen* screen, const vector<Pixel>& leftPixels, const vector<Pixel>& rightPixels, vec3 color);
void DrawPolygon( screen* screen, const vector<vec4>& vertices, vec3 color);

int main( int argc, char* argv[] )
{

  screen *screen = InitializeSDL( SCREEN_WIDTH, SCREEN_HEIGHT, FULLSCREEN_MODE );
  vector<Triangle> triangles;

  LoadTestModel(triangles);

  while( !escape )
    {
      Update();
      Draw(screen, triangles);
      SDL_Renderframe(screen);
    }

  SDL_SaveImage( screen, "screenshot.bmp" );
  KillSDL(screen);
  return 0;
}

/*Place your drawing here*/
void Draw(screen* screen, const vector<Triangle>& triangles)
{
  /* Clear buffer */
  memset(screen->buffer, 0, screen->height*screen->width*sizeof(uint32_t));

  for (uint32_t i=0; i<triangles.size(); ++i){
    vector<vec4> vertices(3);
    vertices[0] = triangles[i].v0;
    vertices[1] = triangles[i].v1;
    vertices[2] = triangles[i].v2;

    // for (int v = 0; v < 3; ++v){
    //   ivec2 projPos;
    //   VertexShader(vertices[v], projPos);
    //   vec3 color(1,1,1);
    //   PutPixelSDL(screen, projPos.x, projPos.y, color);
    // }

    DrawPolygon(screen, vertices, triangles[i].color);
    // DrawPolygonEdges(screen, vertices);

  }
}

void ComputePolygonRows(const vector<Pixel>& vertexPixels, vector<Pixel>& leftPixels, vector<Pixel>& rightPixels) {
  int largestVal = -numeric_limits<int>::max();
  int smallestVal = numeric_limits<int>::max();

  int largeIndex;
  int smallIndex;
  int otherIndex;


  for (int v = 0; v < 3; ++v){
    if (vertexPixels[v].y > largestVal) {
      largestVal = vertexPixels[v].y;
      largeIndex = v;
    }
    if (vertexPixels[v].y < smallestVal) {
      smallestVal = vertexPixels[v].y;
      smallIndex = v;
    }
  }

  for (int i = 0; i < 3; i++) {
    if (i != largeIndex && i != smallIndex){
      otherIndex = i;
      break;
    }
  }
  int rows = largestVal - smallestVal + 1;
  leftPixels.resize(rows);
  rightPixels.resize(rows);

  for (int i = 0; i < rows; i++){
    leftPixels[i].x = +numeric_limits<int>::max();
    leftPixels[i].y = 0;
    leftPixels[i].zinv = 0.0;
    rightPixels[i].y = 0;
    rightPixels[i].zinv = 0.0;
    rightPixels[i].x = -numeric_limits<int>::max();
  }

  int toprows = vertexPixels[largeIndex].y  - vertexPixels[otherIndex].y + 1;
  int botrows = vertexPixels[otherIndex].y  - vertexPixels[smallIndex].y + 1;

  vector<Pixel> edge1(toprows);
  vector<Pixel> edge2(botrows);
  vector<Pixel> bigEdge(rows);

  Interpolate(vertexPixels[otherIndex],vertexPixels[largeIndex], edge1);
  Interpolate(vertexPixels[smallIndex], vertexPixels[otherIndex], edge2);
  Interpolate(vertexPixels[smallIndex],vertexPixels[largeIndex], bigEdge);

  if (edge1[0].x > bigEdge[botrows-1].x){
    for (int i = 0; i < rows; i++){
      if (i < botrows) rightPixels[i] = edge2[i];
      if (i >= botrows) rightPixels[i] = edge1[i-botrows+1];
      if (leftPixels[i].x >= edge2[i].x ) leftPixels[i] = bigEdge[i];
    }
  } else {
    for (int i = 0; i < rows; i++){
      if (i < botrows) leftPixels[i] = edge2[i];
      if (i >= botrows) leftPixels[i] = edge1[i-botrows+1];
      if (rightPixels[i].x <= edge2[i].x ) rightPixels[i] = bigEdge[i];
    }
  }

}
// void ComputePolygonRows(const vector<Pixel>& vertexPixels, vector<Pixel>& leftPixels, vector<Pixel>& rightPixels) {
//   int maxY = -numeric_limits<int>::max();
//   int minY = numeric_limits<int>::max();
//   int midY;
//
//   Pixel highest;
//   Pixel lowest;
//   Pixel midPoint;
//   bool is_left = false;
//
//   for (int i = 0; i < 3; i++) {
//     if (vertexPixels[i].y > maxY) {
//       maxY = vertexPixels[i].y;
//       highest = vertexPixels[i];
//     }
//     if (vertexPixels[i].y < minY) {
//       minY = vertexPixels[i].y;
//       lowest = vertexPixels[i];
//     }
//   }
//
//   for (int i = 0; i < 3; i++) {
//     if (vertexPixels[i].x != highest.x && vertexPixels[i].y != highest.y && vertexPixels[i].zinv != highest.zinv && vertexPixels[i].x != lowest.x && vertexPixels[i].y != lowest.y && vertexPixels[i].zinv != lowest.zinv) {
//       midPoint = vertexPixels[i];
//       midY = vertexPixels[i].y;
//     }
//   }
//
//   int numberOfRows = abs(maxY - minY) + 1;
//   int numberOfhigh2mid = abs(maxY - midY) + 1;
//   int numberOfmid2low = abs(midY - minY) + 1;
//
//   // if (maxY == midY) {
//   //   printf("max x: %d max y:%d mid x: %d, mid y: %d\n", highest.x, highest.y, midPoint.x, midPoint.y);
//   // }
//
//   // if (numberOfRows <= 25) {
//   //   printf("Longest = %d\n", numberOfRows);
//   //   for (int i = 0; i < 3; i++) {
//   //     printf("x: %d, y: %d\n", vertexPixels[i].x, vertexPixels[i].y);
//   //   }
//   // }
//
//   leftPixels.resize(numberOfRows);
//   rightPixels.resize(numberOfRows);
//
//   for (int i = 0; i < numberOfRows; ++i) {
//     leftPixels[i].x = numeric_limits<int>::max();
//     rightPixels[i].x = -numeric_limits<int>::max();
//   }
//
//   vector<Pixel> longest(numberOfRows);
//   vector<Pixel> mid2high(numberOfhigh2mid);
//   vector<Pixel> low2mid(numberOfmid2low);
//
//   printf("Number of rows: %d\n", numberOfRows);
//   printf("Number of low 2 mid: %d\n", numberOfmid2low);
//
//
//   Interpolate(lowest, highest, longest);
//   Interpolate(midPoint, highest, mid2high);
//   Interpolate(lowest, midPoint, low2mid);
//
//
//   if (longest[numberOfmid2low - 1].x < mid2high[0].x) {
//     is_left = true;
//   }
//
//   if (is_left) {
//     leftPixels = longest;
//
//     if (midY == maxY) {
//       rightPixels = low2mid;
//     } else {
//   //
//       for (int i = 0; i < numberOfmid2low; i++) {
//         rightPixels[i] = low2mid[i];
//       }
//   //
//   //     for (int i = numberOfmid2low + 1; i < numberOfRows + 1; i++) {
//   //       rightPixels[i - 1] = mid2high[i - numberOfmid2low];
//   //     }
//   //   }
//   //
//   // } else {
//   //   rightPixels = longest;
//   //
//   //   if (midY == maxY) {
//   //     leftPixels = low2mid;
//   //   } else {
//   //     for (int i = 0; i < numberOfmid2low; i++) {
//   //       leftPixels[i] = low2mid[i];
//   //     }
//   //
//   //     for (int i = numberOfmid2low + 1; i < numberOfRows + 1; i++) {
//   //       leftPixels[i - 1] = mid2high[i - numberOfmid2low];
//   //     }
//   //
//     }
//   }
//
// }

void VertexShader(const vec4& v, Pixel& p){
  vec4 n = camera.basis *(v - camera.position);
  p.x = focal_length*(n[0]/n[2]) + SCREEN_WIDTH/2;
  p.y = focal_length*(n[1]/n[2]) + SCREEN_HEIGHT/2;
  p.zinv = 1/n[2];
}

void Interpolate(Pixel a, Pixel b, vector<Pixel>& result){
  int N = result.size();
  Pixel step;
  step.x = int(b.x-a.x)/float(max(N-1,1));
  step.y = int(b.y-a.y)/float(max(N-1,1));
  step.zinv = float(b.zinv-a.zinv)/float(max(N-1,1));

  // Pixel step = Pixel(b - a)/float(max(N-1, 1));
  Pixel current = a;

  for (int i=0; i<N; i++){
    result[i] = current;
    current.x += step.x;
    current.y += step.y;
    current.zinv += step.zinv;
  }
}

void DrawRows(screen* screen, const vector<Pixel>& leftPixels, const vector<Pixel>& rightPixels, vec3 color) {
  float depthBuffer[SCREEN_HEIGHT][SCREEN_WIDTH];

  for (int y = 0; y < SCREEN_HEIGHT; y++) {
    for (int x = 0; x < SCREEN_WIDTH; x++) {
      depthBuffer[y][x] = 0;
    }
  }


  for (uint i = 0; i < leftPixels.size(); i++) {
    // if (leftPixels[i].y >= 0 && leftPixels[i].y < SCREEN_HEIGHT) {
      int left = leftPixels[i].x;
      int right = rightPixels[i].x;
      for (int j = left; j < right; j++) {
        // if (j >= 0 && j < SCREEN_WIDTH) {
          // if (leftPixels[i].zinv > depthBuffer[leftPixels[i].y][j]) {
            PutPixelSDL(screen, j, leftPixels[i].y, color);
            // depthBuffer[leftPixels[i].y][j] = leftPixels[i].zinv;
          // }
        // }
      }
    // }
  }
}

void DrawPolygon(screen* screen, const vector<vec4>& vertices, vec3 color) {
  int V = vertices.size();

  vector<Pixel> vertexPixels(V);

  for (int i = 0; i < V; i++) {
    VertexShader(vertices[i], vertexPixels[i]);
  }

  vector<Pixel> leftPixels;
  vector<Pixel> rightPixels;
  ComputePolygonRows(vertexPixels, leftPixels, rightPixels);
  DrawRows(screen, leftPixels, rightPixels, color);
}

// void DrawLineSDL(screen* surface, ivec2 a, ivec2 b, vec3 color){
//   ivec2 delta = glm::abs(a-b);
//   int pixels = glm::max(delta.x, delta.y) + 1;
//   vector<ivec2> line(pixels);
//   Interpolate(a, b, line);
//   for (int i = 0; i<pixels; i++){
//     PutPixelSDL(surface, line[i].x, line[i].y, color);
//   }
// }
//
// void DrawPolygonEdges(screen* screen, const vector<vec4>& vertices) {
//   int V = vertices.size();
//   vector<ivec2> projectedVertices(V);
//
//   for (int i=0; i<V; ++i){
//     VertexShader(vertices[i], projectedVertices[i]);
//   }
//   for (int i=0; i<V; ++i){
//     int j = (i+1)%V;
//     vec3 color(1,1,1);
//
//     DrawLineSDL(screen, projectedVertices[i], projectedVertices[j], color);
//   }
// }


mat4 lookAt(vec3 from, vec3 to) {
  vec3 forward = normalize(from - to);
  vec3 right = normalize(cross(vec3(0,1,0), forward));
  vec3 up = cross(forward, right);

  vec4 forward4(forward.x, forward.y, forward.z, 0);
  vec4 right4(right.x, right.y, right.z, 0);
  vec4 up4(up.x, up.y, up.z, 0);
  vec4 id(0, 0, 0, 1);
  mat4 camToWorld(right4, up4, forward4, id);

  // vec4 from4(-from.x, -from.y, -from.z, 1);

  // mat4 position(vec4(1,0,0,0), vec4(0,1,0,0), vec4(0,0,1,0), from4);

  return camToWorld;
}


/*Place updates of parameters here*/
mat4 generateRotation(vec3 a){
  vec4 uno = vec4(cos(a[1])*cos(a[2]), cos(a[1])*sin(a[2]), -sin(a[1]), 0);
  vec4 dos = vec4(-cos(a[0])*sin(a[2]) + sin(a[0]*sin(a[1])*cos(a[2])), cos(a[0])*cos(a[2])+sin(a[0])*sin(a[1])*sin(a[2]), sin(a[0])*cos(a[1]), 0);
  vec4 tres = vec4(sin(a[0])*sin(a[2])+cos(a[0])*sin(a[1])*cos(a[2]), -sin(a[0])*cos(a[2])+cos(a[0])*sin(a[1])*sin(a[2]), cos(a[0])*cos(a[1]), 0);
  vec4 cuatro = vec4(0,0,0,1);
  return (mat4(uno, dos, tres, cuatro));
}


void Update()
{
  static int t = SDL_GetTicks();
  /* Compute frame time */
  int t2 = SDL_GetTicks();
  float dt = float(t2-t);
  t = t2;
  /*Good idea to remove this*/
  // std::cout << "Render time: " << dt << " ms." << std::endl;
  /* Update variables*/
  SDL_Event e;
  mat4 translation(vec4(1,0,0,0), vec4(0,1,0,0), vec4(0,0,1,0), vec4(0,0,0,1));

  while(SDL_PollEvent(&e))
  {
    if (e.type == SDL_KEYDOWN){
    switch (e.key.keysym.sym) {
      case SDLK_ESCAPE:
        escape = true;
        break;
      case SDLK_UP:
        // Move camera forward
        translation[3][2] = 0.1;
        camera.position = translation*camera.position;
        break;
      case SDLK_DOWN:
      // Move camera backward
        translation[3][2] = -0.1;
        camera.position = translation*camera.position;
        break;
      case SDLK_LEFT:
      // Move camera to the left
        translation[3][0] = -0.1;
        camera.position = translation*camera.position;
        break;
      case SDLK_RIGHT:
      // Move camera to the right
        translation[3][0] = 0.1;
        camera.position = translation*camera.position;
        break;
      case SDLK_n:
        translation[3][1] = -0.1;
        camera.position = translation*camera.position;
        break;
      case SDLK_m:
        translation[3][1] = 0.1;
        camera.position = translation * camera.position;
        break;
      case SDLK_d:
        // Rotate camera right;
        camera.basis = translation * generateRotation(vec3(0, yaw, 0)) * camera.basis;
        if (is_lookAt) camera.position = translation * generateRotation(vec3(0, yaw, 0)) * camera.position;
        break;
      case SDLK_a:
        // Rotate camera left;
        camera.basis = translation * generateRotation(vec3(0, -yaw, 0)) * camera.basis;
        if (is_lookAt) camera.position = translation * generateRotation(vec3(0, -yaw, 0)) * camera.position;

        break;
      case SDLK_w:
        // Rotate camera top;
        camera.basis = translation * generateRotation(vec3(yaw, 0, 0)) * camera.basis;
        if (is_lookAt) camera.position = translation * generateRotation(vec3(yaw, 0, 0)) * camera.position;

        break;
      case SDLK_s:
        // Rotate camera down;
        camera.basis = translation * generateRotation(vec3(-yaw, 0, 0)) * camera.basis;
        if (is_lookAt) camera.position = translation * generateRotation(vec3(-yaw, 0, 0)) * camera.position;
        break;
      case SDLK_q:
        camera.basis = translation * generateRotation(vec3(0, 0, -yaw)) * camera.basis;
        if (is_lookAt) camera.position = translation * generateRotation(vec3(0, 0, -yaw)) * camera.position;
        break;
      case SDLK_e:
        camera.basis = translation * generateRotation(vec3(0, 0, yaw)) * camera.basis;
        if (is_lookAt) camera.position = translation * generateRotation(vec3(0, 0, yaw)) * camera.position;
        break;
      case SDLK_l:
        if (is_lookAt) is_lookAt = false;
        else is_lookAt = true;
        break;
      // case SDLK_u:
      //   light.position += vec4(0, 0, 0.1, 0);
      //   break;
      // case SDLK_j:
      //   light.position += vec4(0,0,-0.1,0);
      //   break;
      // case SDLK_h:
      //   light.position += vec4(-0.1, 0, 0, 0);
      //   break;
      // case SDLK_k:
      //   light.position += vec4(0.1, 0, 0, 0);
      //   break;
      default:
        break;
    }

    if (is_lookAt) {
      vec3 position = vec3(camera.position[0], camera.position[1], camera.position[2]);
      camera.basis = lookAt(camera.center, position);
    }
   }
 }
}
