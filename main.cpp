#include <GL/glew.h>	//must be before glfw, because most header files we need are in glew
#include <GLFW/glfw3.h>
#include <iostream>
#include <cstdlib>
#include <string>
#include <fstream>
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <vector>
#include "tiny_obj_loader.h"

#define GLM_FORCE_RADIANS

GLuint quadVAO, quadVBO;
GLuint framebuffer;
GLuint textureColorbuffer;
GLuint rbo;
glm::vec2 circleCenter = glm::vec2(0.5,0.5);
GLfloat radius = 0.1;
GLfloat sigma = 0.8409;
GLfloat quadVertices[] = {   // Vertex attributes for a quad that fills the entire screen in Normalized Device Coordinates.
							 // Positions   // TexCoords
	-1.0f,  1.0f,  0.0f, 1.0f,
	-1.0f, -1.0f,  0.0f, 0.0f,
	1.0f, -1.0f,  1.0f, 0.0f,

	-1.0f,  1.0f,  0.0f, 1.0f,
	1.0f, -1.0f,  1.0f, 0.0f,
	1.0f,  1.0f,  1.0f, 1.0f
};
struct object_struct{
	unsigned int program;
	unsigned int vao;
	unsigned int vbo[4];
	unsigned int texture;
	glm::mat4 model;
	object_struct(): model(glm::mat4(1.0f)){}
} ;

std::vector<object_struct> objects;//vertex array object,vertex buffer object and texture(color) for objs
unsigned int program, program1, program2, program3, screenShader;
std::vector<int> indicesCount;//Number of indice of objs

static void error_callback(int error, const char* description)
{
	fputs(description, stderr);
}
static void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods)
{
	if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
		glfwSetWindowShouldClose(window, GL_TRUE);
	if (key == GLFW_KEY_Y && action == GLFW_RELEASE) {	//按Y增加post processing 的範圍
		radius += 0.01;
		if (radius > 1) {
			radius = 1;
		}
	}
	if (key == GLFW_KEY_T && action == GLFW_RELEASE){	//按T減少post processing 的範圍
		radius -= 0.01;
		if (radius < 0)
			radius = 0;
	}
	if (key == GLFW_KEY_H && action == GLFW_RELEASE) {	//按H增加Gaussian Blur的sigma
		sigma += 0.1;
	}
	if (key == GLFW_KEY_G && action == GLFW_RELEASE) {	//按G減少Gaussian Blur的sigma
		sigma -= 0.1;
	}
}

void mouse_callback(GLFWwindow* window, double xpos, double ypos) {	//將post processing範圍的中心設定為滑鼠位置
	circleCenter.x = xpos/800;
	circleCenter.y = 1 - ypos/600;
}

static unsigned int setup_shader(const char *vertex_shader, const char *fragment_shader)
{
	GLuint vs=glCreateShader(GL_VERTEX_SHADER);
	glShaderSource(vs, 1, (const GLchar**)&vertex_shader, nullptr);

	glCompileShader(vs);	//compile vertex shader

	int status, maxLength;
	char *infoLog=nullptr;
	glGetShaderiv(vs, GL_COMPILE_STATUS, &status);		//get compile status
	if(status==GL_FALSE)								//if compile error
	{
		glGetShaderiv(vs, GL_INFO_LOG_LENGTH, &maxLength);	//get error message length

		/* The maxLength includes the NULL character */
		infoLog = new char[maxLength];
		
		glGetShaderInfoLog(vs, maxLength, &maxLength, infoLog);		//get error message

		fprintf(stderr, "Vertex Shader Error: %s\n", infoLog);

		/* Handle the error in an appropriate way such as displaying a message or writing to a log file. */
		/* In this simple program, we'll just leave */
		delete [] infoLog;
		return 0;
	}
	//	for fragment shader --> same as vertex shader
	GLuint fs=glCreateShader(GL_FRAGMENT_SHADER);
	glShaderSource(fs, 1, (const GLchar**)&fragment_shader, nullptr);
	glCompileShader(fs);

	glGetShaderiv(fs, GL_COMPILE_STATUS, &status);
	if(status==GL_FALSE)
	{
		glGetShaderiv(fs, GL_INFO_LOG_LENGTH, &maxLength);

		/* The maxLength includes the NULL character */
		infoLog = new char[maxLength];

		glGetShaderInfoLog(fs, maxLength, &maxLength, infoLog);

		fprintf(stderr, "Fragment Shader Error: %s\n", infoLog);

		/* Handle the error in an appropriate way such as displaying a message or writing to a log file. */
		/* In this simple program, we'll just leave */
		delete [] infoLog;
		return 0;
	}

	unsigned int program=glCreateProgram();
	// Attach our shaders to our program
	glAttachShader(program, vs);
	glAttachShader(program, fs);

	glLinkProgram(program);

	glGetProgramiv(program, GL_LINK_STATUS, &status);

	if(status==GL_FALSE)
	{
		glGetProgramiv(program, GL_INFO_LOG_LENGTH, &maxLength);


		/* The maxLength includes the NULL character */
		infoLog = new char[maxLength];
		glGetProgramInfoLog(program, maxLength, NULL, infoLog);

		glGetProgramInfoLog(program, maxLength, &maxLength, infoLog);

		fprintf(stderr, "Link Error: %s\n", infoLog);

		/* Handle the error in an appropriate way such as displaying a message or writing to a log file. */
		/* In this simple program, we'll just leave */
		delete [] infoLog;
		return 0;
	}
	return program;
}

static std::string readfile(const char *filename)
{
	std::ifstream ifs(filename);
	if(!ifs)
		exit(EXIT_FAILURE);
	return std::string( (std::istreambuf_iterator<char>(ifs)),
			(std::istreambuf_iterator<char>()));
}

// mini bmp loader written by HSU YOU-LUN
static unsigned char *load_bmp(const char *bmp, unsigned int *width, unsigned int *height, unsigned short int *bits)
{
	unsigned char *result=nullptr;
	FILE *fp = fopen(bmp, "rb");
	if(!fp)
		return nullptr;
	char type[2];
	unsigned int size, offset;
	// check for magic signature	
	fread(type, sizeof(type), 1, fp);
	if(type[0]==0x42 || type[1]==0x4d){
		fread(&size, sizeof(size), 1, fp);
		// ignore 2 two-byte reversed fields
		fseek(fp, 4, SEEK_CUR);
		fread(&offset, sizeof(offset), 1, fp);
		// ignore size of bmpinfoheader field
		fseek(fp, 4, SEEK_CUR);
		fread(width, sizeof(*width), 1, fp);
		fread(height, sizeof(*height), 1, fp);
		// ignore planes field
		fseek(fp, 2, SEEK_CUR);
		fread(bits, sizeof(*bits), 1, fp);
		unsigned char *pos = result = new unsigned char[size-offset];
		fseek(fp, offset, SEEK_SET);
		while(size-ftell(fp)>0)
			pos+=fread(pos, 1, size-ftell(fp), fp);
	}
	fclose(fp);
	return result;
}

static int add_obj(unsigned int program, const char *filename,const char *texbmp)
{
	object_struct new_node;

	std::vector<tinyobj::shape_t> shapes;
	std::vector<tinyobj::material_t> materials;

	std::string err = tinyobj::LoadObj(shapes, materials, filename);

	if (!err.empty()||shapes.size()==0)
	{
		std::cerr<<err<<std::endl;
		exit(1);
	}

	glGenVertexArrays(1, &new_node.vao);
	glGenBuffers(4, new_node.vbo);
	glGenTextures(1, &new_node.texture);

	glBindVertexArray(new_node.vao);

	// Upload postion array
	glBindBuffer(GL_ARRAY_BUFFER, new_node.vbo[0]);
	glBufferData(GL_ARRAY_BUFFER, sizeof(GLfloat)*shapes[0].mesh.positions.size(),
			shapes[0].mesh.positions.data(), GL_STATIC_DRAW);
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, 0);
	if(shapes[0].mesh.texcoords.size()>0)
	{

		// Upload texCoord array
		glBindBuffer(GL_ARRAY_BUFFER, new_node.vbo[1]);
		glBufferData(GL_ARRAY_BUFFER, sizeof(GLfloat)*shapes[0].mesh.texcoords.size(),
				shapes[0].mesh.texcoords.data(), GL_STATIC_DRAW);
		glEnableVertexAttribArray(1);
		glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 0, 0);
		//glActiveTexture(GL_TEXTURE0);	//Activate texture unit before binding texture, used when having multiple texture
		glBindTexture( GL_TEXTURE_2D, new_node.texture);
		unsigned int width, height;
		unsigned short int bits;
		unsigned char *bgr=load_bmp(texbmp, &width, &height, &bits);
		GLenum format = (bits == 24? GL_BGR: GL_BGRA);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, format, GL_UNSIGNED_BYTE, bgr);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_NEAREST);
		glGenerateMipmap(GL_TEXTURE_2D);
		delete [] bgr;
	}

	if(shapes[0].mesh.normals.size()>0)
	{
		// Upload normal array
		glBindBuffer(GL_ARRAY_BUFFER, new_node.vbo[2]);
		glBufferData(GL_ARRAY_BUFFER, sizeof(GLfloat)*shapes[0].mesh.normals.size(),
				shapes[0].mesh.normals.data(), GL_STATIC_DRAW);

		glEnableVertexAttribArray(2);
		glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, 0, 0);
	}

	// Setup index buffer for glDrawElements
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, new_node.vbo[3]);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(GLuint)*shapes[0].mesh.indices.size(),
			shapes[0].mesh.indices.data(), GL_STATIC_DRAW);

	indicesCount.push_back(shapes[0].mesh.indices.size());

	glBindVertexArray(0);

	new_node.program = program;

	objects.push_back(new_node);
	return objects.size()-1;
}

static void releaseObjects()
{
	for(int i=0;i<objects.size();i++){
		glDeleteVertexArrays(1, &objects[i].vao);
		glDeleteTextures(1, &objects[i].texture);
		glDeleteBuffers(4, objects[i].vbo);
	}
	glDeleteProgram(program);
}

static void setUniformMat4(unsigned int program, const std::string &name, const glm::mat4 &mat)
{
	// This line can be ignore. But, if you have multiple shader program
	// You must check if currect binding is the one you want
	glUseProgram(program);
	GLint loc=glGetUniformLocation(program, name.c_str());
	if(loc==-1) return;

	// mat4 of glm is column major, same as opengl
	// we don't need to transpose it. so..GL_FALSE
	glUniformMatrix4fv(loc, 1, GL_FALSE, glm::value_ptr(mat));
}

static void render()
{
	glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
	// Clear all attached buffers        
	glClearColor(0.0f, 0.0f, 0.0f, 1.0f);		//set bg to black
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	glEnable(GL_DEPTH_TEST);
	for (int i = 0; i < objects.size(); i++) {
		//VAO VBO are binded in add_Object function
		glUseProgram(objects[i].program);
		glBindVertexArray(objects[i].vao);
		glBindTexture(GL_TEXTURE_2D, objects[i].texture);
		//you should send some data to shader here
		GLint modelLoc = glGetUniformLocation(objects[i].program, "model");
		glm::mat4 mPosition;
		switch (i) {
		case 0:	//top left
			mPosition = glm::translate(mPosition, glm::vec3(0, 10, 10));
			mPosition = glm::rotate(mPosition, (float)glfwGetTime()*1.0f, glm::vec3(0.0f, 1.0f, 0.0f));
			break;
		case 1: //top right
			mPosition = glm::translate(mPosition, glm::vec3(0, 10, -10));
			mPosition = glm::rotate(mPosition, (float)glfwGetTime()*1.0f, glm::vec3(0.0f, 1.0f, 0.0f));
			break;
		case 2:	//bottom left
			mPosition = glm::translate(mPosition, glm::vec3(0,-10, 10));
			mPosition = glm::rotate(mPosition, (float)glfwGetTime()*1.0f, glm::vec3(0.0f, 1.0f, 0.0f));
			break;
		case 3://bottom right
			mPosition = glm::translate(mPosition, glm::vec3(0, -10, -10));
			mPosition = glm::rotate(mPosition, (float)glfwGetTime()*1.0f, glm::vec3(0.0f, 1.0f, 0.0f));
			break;
		}
		//mPosition = glm::translate(mPosition, modelPositions[i]);
/*		
		else {				//for sun
			mPosition = glm::rotate(mPosition, (float)glfwGetTime()*1.0f, glm::vec3(0.0f, 1.0f, 0.0f));
		}*/
		//mPosition = glm::rotate(mPosition, (float)glfwGetTime()*1.0f, glm::vec3(0.0f, 1.0f, 0.0f));

		//mPosition = glm::scale(glm::mat4(1.0f), glm::vec3(1.5f));
		glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(mPosition));
		//std::cout << i<< std::endl;
		glDrawElements(GL_TRIANGLES, indicesCount[i], GL_UNSIGNED_INT, nullptr);
	}
	
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	// Clear all relevant buffers
	glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT);
	glDisable(GL_DEPTH_TEST); 

							  // Draw Screen
	glUseProgram(screenShader);
	GLuint varLoc = glGetUniformLocation(screenShader, "circleCenter");
	glUniform2f(varLoc, circleCenter.x, circleCenter.y);
	varLoc = glGetUniformLocation(screenShader, "radius");
	glUniform1f(varLoc, radius);
	varLoc = glGetUniformLocation(screenShader, "sigma");
	glUniform1f(varLoc, sigma);
	glBindVertexArray(quadVAO);
	glBindTexture(GL_TEXTURE_2D, textureColorbuffer);	// Use the color attachment texture as the texture of the quad plane
	glDrawArrays(GL_TRIANGLES, 0, 6);
	glBindVertexArray(0);
}
static void set_vp(unsigned int prog) {
	setUniformMat4(prog, "vp", glm::perspective(glm::radians(45.0f), 640.0f / 480, 1.0f, 100.f) *
		glm::lookAt(glm::vec3(40.0f), glm::vec3(), glm::vec3(0, 1, 0)) * glm::mat4(1.0f));
}
static void set_viewPosition(unsigned int prog) {
	glUseProgram(prog);
	GLint loc = glGetUniformLocation(prog, "viewPos");
	glUniform3f(loc, 40.0f, 0, 0);
}
int main(int argc, char *argv[])
{
	GLFWwindow* window;
	glfwSetErrorCallback(error_callback);
	if (!glfwInit())
		exit(EXIT_FAILURE);
	// OpenGL 3.3, Mac OS X is reported to have some problem. However I don't have Mac to test
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);		//set hint to glfwCreateWindow, (target, hintValue)
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
	//hint--> window not resizable,  explicit use core-profile,  opengl version 3.3
	// For Mac OS X
	glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
	window = glfwCreateWindow(800, 600, "Simple Example", NULL, NULL);
	if (!window)
	{
		glfwTerminate();
		return EXIT_FAILURE;
	}

	glfwMakeContextCurrent(window);	//set current window as main window to focus

	// This line MUST put below glfwMakeContextCurrent
	glewExperimental = GL_TRUE;		//tell glew to use more modern technique for managing OpenGL functionality
	glewInit();
	
	// Enable vsync
	glfwSwapInterval(1);
	glProvokingVertex(GL_FIRST_VERTEX_CONVENTION);
	// Setup input callback
	glfwSetKeyCallback(window, key_callback);	//set key event handler
	glfwSetCursorPosCallback(window, mouse_callback);
	// load shader program
	program = setup_shader(readfile("flat_vs.txt").c_str(), readfile("flat_fs.txt").c_str());
	program1 = setup_shader(readfile("gourand_vs.txt").c_str(), readfile("gourand_fs.txt").c_str());
	program2 = setup_shader(readfile("phong_vs.txt").c_str(), readfile("phong_fs.txt").c_str());
	program3 = setup_shader(readfile("blin_vs.txt").c_str(), readfile("blin_fs.txt").c_str());
	screenShader = setup_shader(readfile("screenShader_vs.txt").c_str(), readfile("screenShader_fs.txt").c_str());

	int sun = add_obj(program, "sun.obj","sun.bmp");
	int sun1 = add_obj(program1, "sun.obj", "sun.bmp");
	int sun2 = add_obj(program2, "sun.obj", "sun.bmp");
	int sun3 = add_obj(program3, "sun.obj", "sun.bmp");

	glEnable(GL_DEPTH_TEST);
	// prevent faces rendering to the front while they're behind other faces. 
	 glCullFace(GL_BACK);
	// discard back-facing trangle
	// Enable blend mode for billboard
	//glEnable(GL_BLEND);
	//glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

//-----------------------------------------------------------setting view point----------------------------------//
	 set_vp(program);
	 set_vp(program1);
	 set_vp(program2);
	 set_vp(program3);
//-----------------------------------------------------------end of setting--------------------------------------//	 
//------------------------------------------------------setting viewing position---------------------------------//
	 set_viewPosition(program);
	 set_viewPosition(program1);
	 set_viewPosition(program2);
	 set_viewPosition(program3);
//------------------------------------------------------end of setting-------------------------------------------//
//------------------------------------------------------setting quadVAO VBO--------------------------------------//
	 glGenVertexArrays(1, &quadVAO);
	 glGenBuffers(1, &quadVBO);
	 glBindVertexArray(quadVAO);
	 glBindBuffer(GL_ARRAY_BUFFER, quadVBO);
	 glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices), &quadVertices, GL_STATIC_DRAW);
	 glEnableVertexAttribArray(0);
	 glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(GLfloat), (GLvoid*)0);
	 glEnableVertexAttribArray(1);
	 glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(GLfloat), (GLvoid*)(2 * sizeof(GLfloat)));
	 glBindVertexArray(0);
//------------------------------------------------------end of setting-------------------------------------------//
	 // Framebuffers
	 
	 glGenFramebuffers(1, &framebuffer);
	 glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
	 // Create a color attachment texture
	 
	 glGenTextures(1, &textureColorbuffer);
	 glBindTexture(GL_TEXTURE_2D, textureColorbuffer);
	 glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 800, 600, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
	 glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	 glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	 glBindTexture(GL_TEXTURE_2D, 0);

	 glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, textureColorbuffer, 0);
	 // Create a renderbuffer object for depth and stencil attachment
	
	 glGenRenderbuffers(1, &rbo);
	 glBindRenderbuffer(GL_RENDERBUFFER, rbo);
	 glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, 800, 600); // Use a single renderbuffer object for both a depth AND stencil buffer.
	 glBindRenderbuffer(GL_RENDERBUFFER, 0);
	 glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, rbo); //attach frameBuffer and renderBuffer
																								   //check if it is actually complete
	 if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
		 std::cout << "ERROR::FRAMEBUFFER:: Framebuffer is not complete!" << std::endl;
	 glBindFramebuffer(GL_FRAMEBUFFER, 0);
	float last, start;
	last = start = glfwGetTime();
	int fps=0;
	while (!glfwWindowShouldClose(window))
	{//program will keep draw here until you close the window
		float delta = glfwGetTime() - start;
		
		render();
		glfwSwapBuffers(window);	//swap the color buffer and show it as output to the screen.
		glfwPollEvents();			//check if there is any event being triggered
		fps++;
		if(glfwGetTime() - last > 1.0)
		{
			std::cout<<(double)fps/(glfwGetTime()-last)<<std::endl;
			fps = 0;
			last = glfwGetTime();
		}
	}

	releaseObjects();
	glfwDestroyWindow(window);
	glfwTerminate();
	return EXIT_SUCCESS;
}
