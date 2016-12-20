#include "Base.h"
#include "Effect.h"
#include "FileSystem.h"
#include "Game.h"
#include "vulkantools.h"

#define OPENGL_ES_DEFINE  "OPENGL_ES"

namespace vkcore
{


// Cache of unique effects.
static std::map<std::string, Effect*> __effectCache;
static Effect* __currentEffect = NULL;


static void replaceDefines(const char* defines, std::string& out)
{
	Properties* graphicsConfig = Game::getInstance()->getConfig()->getNamespace("graphics", true);
	const char* globalDefines = graphicsConfig ? graphicsConfig->getString("shaderDefines") : NULL;

	// Build full semicolon delimited list of defines
#ifdef OPENGL_ES
	out = OPENGL_ES_DEFINE;
#else
	out = "";
#endif
	if (globalDefines && strlen(globalDefines) > 0)
	{
		if (out.length() > 0)
			out += ';';
		out += globalDefines;
	}
	if (defines && strlen(defines) > 0)
	{
		if (out.length() > 0)
			out += ';';
		out += defines;
	}

	// Replace semicolons
	if (out.length() > 0)
	{
		size_t pos;
		out.insert(0, "#define ");
		while ((pos = out.find(';')) != std::string::npos)
		{
			out.replace(pos, 1, "\n#define ");
		}
		out += "\n";
	}
}

static void replaceIncludes(const char* filepath, const char* source, std::string& out)
{
	// Replace the #include "xxxx.xxx" with the sourced file contents of "filepath/xxxx.xxx"
	std::string str = source;
	size_t lastPos = 0;
	size_t headPos = 0;
	size_t fileLen = str.length();
	size_t tailPos = fileLen;
	while (headPos < fileLen)
	{
		lastPos = headPos;
		if (headPos == 0)
		{
			// find the first "#include"
			headPos = str.find("#include");
		}
		else
		{
			// find the next "#include"
			headPos = str.find("#include", headPos + 1);
		}

		// If "#include" is found
		if (headPos != std::string::npos)
		{
			// append from our last position for the legth (head - last position) 
			out.append(str.substr(lastPos, headPos - lastPos));

			// find the start quote "
			size_t startQuote = str.find("\"", headPos) + 1;
			if (startQuote == std::string::npos)
			{
				// We have started an "#include" but missing the leading quote "
				GP_ERROR("Compile failed for shader '%s' missing leading \".", filepath);
				return;
			}
			// find the end quote "
			size_t endQuote = str.find("\"", startQuote);
			if (endQuote == std::string::npos)
			{
				// We have a start quote but missing the trailing quote "
				GP_ERROR("Compile failed for shader '%s' missing trailing \".", filepath);
				return;
			}

			// jump the head position past the end quote
			headPos = endQuote + 1;

			// File path to include and 'stitch' in the value in the quotes to the file path and source it.
			std::string filepathStr = filepath;
			std::string directoryPath = filepathStr.substr(0, filepathStr.rfind('/') + 1);
			size_t len = endQuote - (startQuote);
			std::string includeStr = str.substr(startQuote, len);
			directoryPath.append(includeStr);
			const char* includedSource = FileSystem::readAll(directoryPath.c_str());
			if (includedSource == NULL)
			{
				GP_ERROR("Compile failed for shader '%s' invalid filepath.", filepathStr.c_str());
				return;
			}
			else
			{
				// Valid file so lets attempt to see if we need to append anything to it too (recurse...)
				replaceIncludes(directoryPath.c_str(), includedSource, out);
				SAFE_DELETE_ARRAY(includedSource);
			}
		}
		else
		{
			// Append the remaining
			out.append(str.c_str(), lastPos, tailPos);
		}
	}
}

static void writeShaderToErrorFile(const char* filePath, const char* source)
{
	std::string path = filePath;
	path += ".err";
	std::unique_ptr<Stream> stream(FileSystem::open(path.c_str(), FileSystem::WRITE));
	if (stream.get() != NULL && stream->canWrite())
	{
		stream->write(source, 1, strlen(source));
	}
}


Effect::Effect() : _program(0)
{
}

Effect::~Effect()
{
    // Remove this effect from the cache.
    __effectCache.erase(_id);

    // Free uniforms.
    for (std::map<std::string, Uniform*>::iterator itr = _uniforms.begin(); itr != _uniforms.end(); ++itr)
    {
        SAFE_DELETE(itr->second);
    }

    if (_program)
    {
        // If our program object is currently bound, unbind it before we're destroyed.
        if (__currentEffect == this)
        {
            GL_ASSERT( glUseProgram(0) );
            __currentEffect = NULL;
        }

        GL_ASSERT( glDeleteProgram(_program) );
        _program = 0;
    }
}

Effect* Effect::createFromFile(const char* vshPath, const char* fshPath, const char* defines)
{
    GP_ASSERT(vshPath);
    GP_ASSERT(fshPath);

    // Search the effect cache for an identical effect that is already loaded.
    std::string uniqueId = vshPath;
    uniqueId += ';';
    uniqueId += fshPath;
    uniqueId += ';';
    if (defines)
    {
        uniqueId += defines;
    }
    std::map<std::string, Effect*>::const_iterator itr = __effectCache.find(uniqueId);
    if (itr != __effectCache.end())
    {
        // Found an exiting effect with this id, so increase its ref count and return it.
        GP_ASSERT(itr->second);
        itr->second->addRef();
        return itr->second;
    }

    // Read source from file.
    char* vshSource = FileSystem::readAll(vshPath);
    if (vshSource == NULL)
    {
        GP_ERROR("Failed to read vertex shader from file '%s'.", vshPath);
        return NULL;
    }
    char* fshSource = FileSystem::readAll(fshPath);
    if (fshSource == NULL)
    {
        GP_ERROR("Failed to read fragment shader from file '%s'.", fshPath);
        SAFE_DELETE_ARRAY(vshSource);
        return NULL;
    }

    Effect* effect = createFromSource(vshPath, vshSource, fshPath, fshSource, defines);
    
    SAFE_DELETE_ARRAY(vshSource);
    SAFE_DELETE_ARRAY(fshSource);

    if (effect == NULL)
    {
        GP_ERROR("Failed to create effect from shaders '%s', '%s'.", vshPath, fshPath);
    }
    else
    {
        // Store this effect in the cache.
        effect->_id = uniqueId;
        __effectCache[uniqueId] = effect;
    }

    return effect;
}

Effect* Effect::createFromSource(const char* vshSource, const char* fshSource, const char* defines)
{
    return createFromSource(NULL, vshSource, NULL, fshSource, defines);
}

Effect* Effect::createFromSource(const char* vshPath, const char* vshSource, const char* fshPath,
	const char* fshSource, const char* defines)
{
    GP_ASSERT(vshSource);
    GP_ASSERT(fshSource);

	// Create and return the new Effect.
	Effect* effect = new Effect();

    std::string shaderSource;

    // Replace all comma separated definitions with #define prefix and \n suffix
    std::string definesStr = "";
    replaceDefines(defines, definesStr);
    
    shaderSource = definesStr.c_str();
    shaderSource += "\n";
    std::string vshSourceStr = "";
    if (vshPath)
    {
        replaceIncludes(vshPath, vshSource, vshSourceStr);
        if (vshSource && strlen(vshSource) != 0)
            vshSourceStr += "\n";
    }
    shaderSource += (vshPath ? vshSourceStr.c_str() :  vshSource);

	effect->shaderStages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	effect->shaderStages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;

	VkShaderModule shaderModule;
	VkShaderModuleCreateInfo moduleCreateInfo;
	moduleCreateInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	moduleCreateInfo.pNext = NULL;
	moduleCreateInfo.codeSize = 3 * sizeof(uint32_t) + shaderSource.length() + 1;
	moduleCreateInfo.pCode = (uint32_t*)malloc(moduleCreateInfo.codeSize);
	moduleCreateInfo.flags = 0;

	// Magic SPV number
	((uint32_t *)moduleCreateInfo.pCode)[0] = 0x07230203;
	((uint32_t *)moduleCreateInfo.pCode)[1] = 0;
	((uint32_t *)moduleCreateInfo.pCode)[2] = VK_SHADER_STAGE_VERTEX_BIT;
	memcpy(((uint32_t *)moduleCreateInfo.pCode + 3), shaderSource.c_str(), shaderSource.length() + 1);

	VK_CHECK_RESULT(vkCreateShaderModule(gVulkanDevice->mLogicalDevice, &moduleCreateInfo, NULL, &shaderModule));
	effect->shaderStages[0].module = shaderModule;
	effect->shaderStages[0].pName = "main"; // todo : make param
	effect->shaderModules.push_back(effect->shaderStages[0].module);

   ////////////////////////////////////////////////////////////

	shaderSource.clear();
	shaderSource = definesStr.c_str();
	shaderSource += "\n";
    // Compile the fragment shader.
    std::string fshSourceStr;
    if (fshPath)
    {
        replaceIncludes(fshPath, fshSource, fshSourceStr);
        if (fshSource && strlen(fshSource) != 0)
            fshSourceStr += "\n";
    }

    shaderSource = (fshPath ? fshSourceStr.c_str() : fshSource);
	effect->shaderStages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	effect->shaderStages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;

	VkShaderModule shaderModuleFra;
	VkShaderModuleCreateInfo moduleCreateInfoFra;
	moduleCreateInfoFra.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	moduleCreateInfoFra.pNext = NULL;
	moduleCreateInfoFra.codeSize = 3 * sizeof(uint32_t) + shaderSource.length() + 1;
	moduleCreateInfoFra.pCode = (uint32_t*)malloc(moduleCreateInfoFra.codeSize);
	moduleCreateInfoFra.flags = 0;

	// Magic SPV number
	((uint32_t *)moduleCreateInfoFra.pCode)[0] = 0x07230203;
	((uint32_t *)moduleCreateInfoFra.pCode)[1] = 0;
	((uint32_t *)moduleCreateInfoFra.pCode)[2] = VK_SHADER_STAGE_FRAGMENT_BIT;
	memcpy(((uint32_t *)moduleCreateInfoFra.pCode + 3), shaderSource.c_str(), shaderSource.length() + 1);
	VK_CHECK_RESULT(vkCreateShaderModule(gVulkanDevice->mLogicalDevice, &moduleCreateInfoFra, NULL, &shaderModuleFra));

	effect->shaderStages[1].module = shaderModuleFra;
	effect->shaderStages[1].pName = "main"; // todo : make param
	effect->shaderModules.push_back(effect->shaderStages[1].module);

	effect->createPipelineLayout();
  

    return effect;
}

void Effect::createPipelineLayout()
{
	//////////////////////////////
	// Binding 0: Uniform buffer (Vertex shader)
	VkDescriptorSetLayoutBinding layoutBinding = {};
	layoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	layoutBinding.descriptorCount = 1;
	layoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
	layoutBinding.pImmutableSamplers = nullptr;

	VkDescriptorSetLayoutCreateInfo descriptorLayout = {};
	descriptorLayout.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	descriptorLayout.pNext = nullptr;
	descriptorLayout.bindingCount = 1;
	descriptorLayout.pBindings = &layoutBinding;

	VK_CHECK_RESULT(vkCreateDescriptorSetLayout(gVulkanDevice->mLogicalDevice, &descriptorLayout, nullptr, &mDescriptorSetLayout));

	// Create the pipeline layout that is used to generate the rendering pipelines that are based on this descriptor set layout
	// In a more complex scenario you would have different pipeline layouts for different descriptor set layouts that could be reused
	VkPipelineLayoutCreateInfo pPipelineLayoutCreateInfo = {};
	pPipelineLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	pPipelineLayoutCreateInfo.pNext = nullptr;
	pPipelineLayoutCreateInfo.setLayoutCount = 1;
	pPipelineLayoutCreateInfo.pSetLayouts = &mDescriptorSetLayout;

	VK_CHECK_RESULT(vkCreatePipelineLayout(gVulkanDevice->mLogicalDevice, &pPipelineLayoutCreateInfo, nullptr, &mPipelineLayout));
	return;
}

const char* Effect::getId() const
{
    return _id.c_str();
}

VertexAttribute Effect::getVertexAttribute(const char* name) const
{
    std::map<std::string, VertexAttribute>::const_iterator itr = _vertexAttributes.find(name);
    return (itr == _vertexAttributes.end() ? -1 : itr->second);
}

Uniform* Effect::getUniform(const char* name) const
{
    std::map<std::string, Uniform*>::const_iterator itr = _uniforms.find(name);

	if (itr != _uniforms.end()) {
		// Return cached uniform variable
		return itr->second;
	}

    GLint uniformLocation;
    GL_ASSERT( uniformLocation = glGetUniformLocation(_program, name) );
    if (uniformLocation > -1)
	{
		// Check for array uniforms ("u_directionalLightColor[0]" -> "u_directionalLightColor")
		char* parentname = new char[strlen(name)+1];
		strcpy(parentname, name);
		if (strtok(parentname, "[") != NULL) {
			std::map<std::string, Uniform*>::const_iterator itr = _uniforms.find(parentname);
			if (itr != _uniforms.end()) {
				Uniform* puniform = itr->second;

				Uniform* uniform = new Uniform();
				uniform->_effect = const_cast<Effect*>(this);
				uniform->_name = name;
				uniform->_location = uniformLocation;
				uniform->_index = 0;
				uniform->_type = puniform->getType();
				_uniforms[name] = uniform;

				SAFE_DELETE_ARRAY(parentname);
				return uniform;
			}
		}
		SAFE_DELETE_ARRAY(parentname);
    }

	// No uniform variable found - return NULL
	return NULL;
}

Uniform* Effect::getUniform(unsigned int index) const
{
    unsigned int i = 0;
    for (std::map<std::string, Uniform*>::const_iterator itr = _uniforms.begin(); itr != _uniforms.end(); ++itr, ++i)
    {
        if (i == index)
        {
            return itr->second;
        }
    }
    return NULL;
}

unsigned int Effect::getUniformCount() const
{
    return (unsigned int)_uniforms.size();
}

void Effect::setValue(Uniform* uniform, float value)
{
    GP_ASSERT(uniform);
    GL_ASSERT( glUniform1f(uniform->_location, value) );
}

void Effect::setValue(Uniform* uniform, const float* values, unsigned int count)
{
    GP_ASSERT(uniform);
    GP_ASSERT(values);
    GL_ASSERT( glUniform1fv(uniform->_location, count, values) );
}

void Effect::setValue(Uniform* uniform, int value)
{
    GP_ASSERT(uniform);
    GL_ASSERT( glUniform1i(uniform->_location, value) );
}

void Effect::setValue(Uniform* uniform, const int* values, unsigned int count)
{
    GP_ASSERT(uniform);
    GP_ASSERT(values);
    GL_ASSERT( glUniform1iv(uniform->_location, count, values) );
}

void Effect::setValue(Uniform* uniform, const Matrix& value)
{
    GP_ASSERT(uniform);
    GL_ASSERT( glUniformMatrix4fv(uniform->_location, 1, GL_FALSE, value.m) );
}

void Effect::setValue(Uniform* uniform, const Matrix* values, unsigned int count)
{
    GP_ASSERT(uniform);
    GP_ASSERT(values);
    GL_ASSERT( glUniformMatrix4fv(uniform->_location, count, GL_FALSE, (GLfloat*)values) );
}

void Effect::setValue(Uniform* uniform, const Vector2& value)
{
    GP_ASSERT(uniform);
    GL_ASSERT( glUniform2f(uniform->_location, value.x, value.y) );
}

void Effect::setValue(Uniform* uniform, const Vector2* values, unsigned int count)
{
    GP_ASSERT(uniform);
    GP_ASSERT(values);
    GL_ASSERT( glUniform2fv(uniform->_location, count, (GLfloat*)values) );
}

void Effect::setValue(Uniform* uniform, const Vector3& value)
{
    GP_ASSERT(uniform);
    GL_ASSERT( glUniform3f(uniform->_location, value.x, value.y, value.z) );
}

void Effect::setValue(Uniform* uniform, const Vector3* values, unsigned int count)
{
    GP_ASSERT(uniform);
    GP_ASSERT(values);
    GL_ASSERT( glUniform3fv(uniform->_location, count, (GLfloat*)values) );
}

void Effect::setValue(Uniform* uniform, const Vector4& value)
{
    GP_ASSERT(uniform);
    GL_ASSERT( glUniform4f(uniform->_location, value.x, value.y, value.z, value.w) );
}

void Effect::setValue(Uniform* uniform, const Vector4* values, unsigned int count)
{
    GP_ASSERT(uniform);
    GP_ASSERT(values);
    GL_ASSERT( glUniform4fv(uniform->_location, count, (GLfloat*)values) );
}

void Effect::setValue(Uniform* uniform, const Texture::Sampler* sampler)
{
    GP_ASSERT(uniform);
    GP_ASSERT(uniform->_type == GL_SAMPLER_2D || uniform->_type == GL_SAMPLER_CUBE);
    GP_ASSERT(sampler);
    GP_ASSERT((sampler->getTexture()->getType() == Texture::TEXTURE_2D && uniform->_type == GL_SAMPLER_2D) || 
        (sampler->getTexture()->getType() == Texture::TEXTURE_CUBE && uniform->_type == GL_SAMPLER_CUBE));

    GL_ASSERT( glActiveTexture(GL_TEXTURE0 + uniform->_index) );

    // Bind the sampler - this binds the texture and applies sampler state
    const_cast<Texture::Sampler*>(sampler)->bind();

    GL_ASSERT( glUniform1i(uniform->_location, uniform->_index) );
}

void Effect::setValue(Uniform* uniform, const Texture::Sampler** values, unsigned int count)
{
    GP_ASSERT(uniform);
    GP_ASSERT(uniform->_type == GL_SAMPLER_2D || uniform->_type == GL_SAMPLER_CUBE);
    GP_ASSERT(values);

    // Set samplers as active and load texture unit array
    GLint units[32];
    for (unsigned int i = 0; i < count; ++i)
    {
        GP_ASSERT((const_cast<Texture::Sampler*>(values[i])->getTexture()->getType() == Texture::TEXTURE_2D && uniform->_type == GL_SAMPLER_2D) || 
            (const_cast<Texture::Sampler*>(values[i])->getTexture()->getType() == Texture::TEXTURE_CUBE && uniform->_type == GL_SAMPLER_CUBE));
        GL_ASSERT( glActiveTexture(GL_TEXTURE0 + uniform->_index + i) );

        // Bind the sampler - this binds the texture and applies sampler state
        const_cast<Texture::Sampler*>(values[i])->bind();

        units[i] = uniform->_index + i;
    }

    // Pass texture unit array to GL
    GL_ASSERT( glUniform1iv(uniform->_location, count, units) );
}

void Effect::bind()
{
   GL_ASSERT( glUseProgram(_program) );

    __currentEffect = this;
}

Effect* Effect::getCurrentEffect()
{
    return __currentEffect;
}

Uniform::Uniform() :
    _location(-1), _type(0), _index(0), _effect(NULL)
{
}

Uniform::~Uniform()
{
    // hidden
}

Effect* Uniform::getEffect() const
{
    return _effect;
}

const char* Uniform::getName() const
{
    return _name.c_str();
}

const GLenum Uniform::getType() const
{
    return _type;
}

}
