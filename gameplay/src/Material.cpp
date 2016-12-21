#include "Base.h"
#include "Material.h"
#include "FileSystem.h"
#include "Effect.h"
#include "Technique.h"
#include "Pass.h"
#include "Properties.h"
#include "Node.h"
#include "Game.h"

namespace vkcore
{


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


	static bool isMaterialKeyword(const char* str)
	{
		GP_ASSERT(str);

#define MATERIAL_KEYWORD_COUNT 3
		static const char* reservedKeywords[MATERIAL_KEYWORD_COUNT] =
		{
			"vertexShader",
			"fragmentShader",
			"defines"
		};
		for (unsigned int i = 0; i < MATERIAL_KEYWORD_COUNT; ++i)
		{
			if (strcmp(reservedKeywords[i], str) == 0)
			{
				return true;
			}
		}
		return false;
	}

	static Texture::Filter parseTextureFilterMode(const char* str, Texture::Filter defaultValue)
	{
		if (str == NULL || strlen(str) == 0)
		{
			GP_ERROR("Texture filter mode string must be non-null and non-empty.");
			return defaultValue;
		}
		else if (strcmp(str, "NEAREST") == 0)
		{
			return Texture::NEAREST;
		}
		else if (strcmp(str, "LINEAR") == 0)
		{
			return Texture::LINEAR;
		}
		else if (strcmp(str, "NEAREST_MIPMAP_NEAREST") == 0)
		{
			return Texture::NEAREST_MIPMAP_NEAREST;
		}
		else if (strcmp(str, "LINEAR_MIPMAP_NEAREST") == 0)
		{
			return Texture::LINEAR_MIPMAP_NEAREST;
		}
		else if (strcmp(str, "NEAREST_MIPMAP_LINEAR") == 0)
		{
			return Texture::NEAREST_MIPMAP_LINEAR;
		}
		else if (strcmp(str, "LINEAR_MIPMAP_LINEAR") == 0)
		{
			return Texture::LINEAR_MIPMAP_LINEAR;
		}
		else
		{
			GP_ERROR("Unsupported texture filter mode string ('%s').", str);
			return defaultValue;
		}
	}

	static Texture::Wrap parseTextureWrapMode(const char* str, Texture::Wrap defaultValue)
	{
		if (str == NULL || strlen(str) == 0)
		{
			GP_ERROR("Texture wrap mode string must be non-null and non-empty.");
			return defaultValue;
		}
		else if (strcmp(str, "REPEAT") == 0)
		{
			return Texture::REPEAT;
		}
		else if (strcmp(str, "CLAMP") == 0)
		{
			return Texture::CLAMP;
		}
		else
		{
			GP_ERROR("Unsupported texture wrap mode string ('%s').", str);
			return defaultValue;
		}
	}


Material::Material()
{
}

Material::~Material()
{
	for (auto& shaderModule : shaderModules)
	{
		vkDestroyShaderModule(gVulkanDevice->mLogicalDevice, shaderModule, nullptr);
	}
	vkDestroyBuffer(gVulkanDevice->mLogicalDevice, mUniformDataVS.buffer, nullptr);
	vkFreeMemory(gVulkanDevice->mLogicalDevice, mUniformDataVS.memory, nullptr);
	vkDestroyPipelineLayout(gVulkanDevice->mLogicalDevice, mPipelineLayout, nullptr);
	vkDestroyDescriptorSetLayout(gVulkanDevice->mLogicalDevice, mDescriptorSetLayout, nullptr);
}

Material* Material::create(const char* url)
{
    return create(url, (PassCallback)NULL, NULL);
}

Material* Material::create(const char* url, PassCallback callback, void* cookie)
{
    // Load the material properties from file.
    Properties* properties = Properties::create(url);
    if (properties == NULL)
    {
        GP_WARN("Failed to create material from file: %s", url);
        return NULL;
    }

    Material* material = create((strlen(properties->getNamespace()) > 0) ? properties : properties->getNextNamespace(), callback, cookie);
    SAFE_DELETE(properties);

    return material;
}

Material* Material::create(Properties* materialProperties)
{
    return create(materialProperties, (PassCallback)NULL, NULL);
}

Material* Material::create(Properties* materialProperties, PassCallback callback, void* cookie)
{
    // Check if the Properties is valid and has a valid namespace.
    if (!materialProperties || !(strcmp(materialProperties->getNamespace(), "material") == 0))
    {
        GP_ERROR("Properties object must be non-null and have namespace equal to 'material'.");
        return NULL;
    }

    // Create new material from the file passed in.
    Material* material = new Material();

    // Load uniform value parameters for this material.
    loadRenderState(material, materialProperties);

    return material;
}

VkPipelineShaderStageCreateInfo Material::loadShader(std::string fileName, VkShaderStageFlagBits stage)
{
	VkPipelineShaderStageCreateInfo shaderStage = {};
	shaderStage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	shaderStage.stage = stage;
	shaderStage.module = vkTools::loadShaderGLSL(fileName.c_str(), gVulkanDevice->mLogicalDevice, stage);
	shaderStage.pName = "main"; // todo : make param
	assert(shaderStage.module != NULL);
	shaderModules.push_back(shaderStage.module);
	return shaderStage;
}


Material* Material::create(const char* vshPath, const char* fshPath, const char* defines)
{
    GP_ASSERT(vshPath);
    GP_ASSERT(fshPath);

    Material* material = new Material();
	material->shaderStages[0] = material->loadShader(vshPath, VK_SHADER_STAGE_VERTEX_BIT);
	material->shaderStages[1] = material->loadShader(fshPath, VK_SHADER_STAGE_FRAGMENT_BIT);

	material->createPipelineLayout();

    return material;
}


void Material::setNodeBinding(Node* node)
{
    RenderState::setNodeBinding(node);
}

void Material::prepareUniformBuffers()
{
	// Prepare and initialize a uniform buffer block containing shader uniforms
	// Single uniforms like in OpenGL are no longer present in Vulkan. All Shader uniforms are passed via uniform buffer blocks
	VkMemoryRequirements memReqs;

	// Vertex shader uniform buffer block
	VkBufferCreateInfo bufferInfo = {};
	VkMemoryAllocateInfo allocInfo = {};
	allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	allocInfo.pNext = nullptr;
	allocInfo.allocationSize = 0;
	allocInfo.memoryTypeIndex = 0;

	bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	bufferInfo.size = sizeof(mUboVS);
	// This buffer will be used as a uniform buffer
	bufferInfo.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;

	// Create a new buffer
	VK_CHECK_RESULT(vkCreateBuffer(gVulkanDevice->mLogicalDevice, &bufferInfo, nullptr, &mUniformDataVS.buffer));
	vkGetBufferMemoryRequirements(gVulkanDevice->mLogicalDevice, mUniformDataVS.buffer, &memReqs);
	allocInfo.allocationSize = memReqs.size;
	allocInfo.memoryTypeIndex = gVulkanDevice->getMemoryTypeIndex(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
	VK_CHECK_RESULT(vkAllocateMemory(gVulkanDevice->mLogicalDevice, &allocInfo, nullptr, &(mUniformDataVS.memory)));
	VK_CHECK_RESULT(vkBindBufferMemory(gVulkanDevice->mLogicalDevice, mUniformDataVS.buffer, mUniformDataVS.memory, 0));

	// Store information in the uniform's descriptor that is used by the descriptor set
	mUniformDataVS.descriptor.buffer = mUniformDataVS.buffer;
	mUniformDataVS.descriptor.offset = 0;
	mUniformDataVS.descriptor.range = sizeof(mUboVS);
}

void Material::updateUniformBuffers(Matrix *promat, Matrix *modelmat, Matrix *viewmat)
{
	mUboVS.projectionMatrix = *promat;
	mUboVS.modelMatrix = *modelmat;
	mUboVS.viewMatrix = *viewmat;
	//// Update matrices
	//float aspect = (float)800 / (float)600;
	//Matrix::createOrthographic(aspect, 1.0f, -1.0f, 1.0f, &mUboVS.projectionMatrix);
	//vkcore::Matrix::createPerspectiveVK(MATH_DEG_TO_RAD(60.0f), 1.0f, 0.1f, 256.0f, &mUboVS.projectionMatrix);
	//Matrix::createTranslation(0.0f, 0.0f, -10.0f, &mUboVS.viewMatrix);
	//Matrix::createRotationX(0.0f, &mUboVS.modelMatrix);
	//mUboVS.modelMatrix.rotateY(0.0f);
	//mUboVS.modelMatrix.rotateZ(0.0f);
	uint8_t *pData;
	VK_CHECK_RESULT(vkMapMemory(gVulkanDevice->mLogicalDevice, mUniformDataVS.memory, 0, sizeof(mUboVS), 0, (void **)&pData));
	memcpy(pData, &mUboVS, sizeof(mUboVS));
	vkUnmapMemory(gVulkanDevice->mLogicalDevice, mUniformDataVS.memory);
}

void Material::updateUniformProMat(Matrix *proMat)
{
	mUboVS.projectionMatrix = *proMat;
	uint8_t *pData;
	VK_CHECK_RESULT(vkMapMemory(gVulkanDevice->mLogicalDevice, mUniformDataVS.memory, 0, sizeof(mUboVS), 0, (void **)&pData));
	memcpy(pData, &mUboVS, sizeof(mUboVS));
	vkUnmapMemory(gVulkanDevice->mLogicalDevice, mUniformDataVS.memory);
}


void Material::createPipelineLayout()
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

Material* Material::clone(NodeCloneContext &context) const
{
    Material* material = new Material();
    RenderState::cloneInto(material, context);

    return material;
}


bool Material::loadPass(Technique* technique, Properties* passProperties, PassCallback callback, void* cookie)
{
    GP_ASSERT(passProperties);
    GP_ASSERT(technique);

    // Fetch shader info required to create the effect of this technique.
    const char* vertexShaderPath = passProperties->getString("vertexShader");
    GP_ASSERT(vertexShaderPath);
    const char* fragmentShaderPath = passProperties->getString("fragmentShader");
    GP_ASSERT(fragmentShaderPath);
    const char* passDefines = passProperties->getString("defines");

    // Create the pass
    Pass* pass = new Pass(passProperties->getId(), technique);

    // Load render state.
    loadRenderState(pass, passProperties);

    // If a pass callback was specified, call it and add the result to our list of defines
    std::string allDefines = passDefines ? passDefines : "";
    if (callback)
    {
        std::string customDefines = callback(pass, cookie);
        if (customDefines.length() > 0)
        {
            if (allDefines.length() > 0)
                allDefines += ';';
            allDefines += customDefines;
        }
    }

    // Initialize/compile the effect with the full set of defines
    if (!pass->initialize(vertexShaderPath, fragmentShaderPath, allDefines.c_str()))
    {
        GP_WARN("Failed to create pass for technique.");
        SAFE_RELEASE(pass);
        return false;
    }

    // Add the new pass to the technique.
    technique->_passes.push_back(pass);

    return true;
}

void Material::loadRenderState(RenderState* renderState, Properties* properties)
{
    GP_ASSERT(renderState);
    GP_ASSERT(properties);

    // Rewind the properties to start reading from the start.
    properties->rewind();

    const char* name;
    while ((name = properties->getNextProperty()))
    {
        if (isMaterialKeyword(name))
            continue; // keyword - skip

        switch (properties->getType())
        {
        case Properties::NUMBER:
            GP_ASSERT(renderState->getParameter(name));
            renderState->getParameter(name)->setValue(properties->getFloat());
            break;
        case Properties::VECTOR2:
            {
                Vector2 vector2;
                if (properties->getVector2(NULL, &vector2))
                {
                    GP_ASSERT(renderState->getParameter(name));
                    renderState->getParameter(name)->setValue(vector2);
                }
            }
            break;
        case Properties::VECTOR3:
            {
                Vector3 vector3;
                if (properties->getVector3(NULL, &vector3))
                {
                    GP_ASSERT(renderState->getParameter(name));
                    renderState->getParameter(name)->setValue(vector3);
                }
            }
            break;
        case Properties::VECTOR4:
            {
                Vector4 vector4;
                if (properties->getVector4(NULL, &vector4))
                {
                    GP_ASSERT(renderState->getParameter(name));
                    renderState->getParameter(name)->setValue(vector4);
                }
            }
            break;
        case Properties::MATRIX:
            {
                Matrix matrix;
                if (properties->getMatrix(NULL, &matrix))
                {
                    GP_ASSERT(renderState->getParameter(name));
                    renderState->getParameter(name)->setValue(matrix);
                }
            }
            break;
        default:
            {
                // Assume this is a parameter auto-binding.
                renderState->setParameterAutoBinding(name, properties->getString());
            }
            break;
        }
    }

    // Iterate through all child namespaces searching for samplers and render state blocks.
    Properties* ns;
    while ((ns = properties->getNextNamespace()))
    {
        if (strcmp(ns->getNamespace(), "sampler") == 0)
        {
            // Read the texture uniform name.
            name = ns->getId();
            if (strlen(name) == 0)
            {
                GP_ERROR("Texture sampler is missing required uniform name.");
                continue;
            }

            // Get the texture path.
            std::string path;
            if (!ns->getPath("path", &path))
            {
                GP_ERROR("Texture sampler '%s' is missing required image file path.", name);
                continue;
            }

            // Read texture state (booleans default to 'false' if not present).
            bool mipmap = ns->getBool("mipmap");
            Texture::Wrap wrapS = parseTextureWrapMode(ns->getString("wrapS"), Texture::REPEAT);
            Texture::Wrap wrapT = parseTextureWrapMode(ns->getString("wrapT"), Texture::REPEAT);
            Texture::Wrap wrapR = Texture::REPEAT;
            if(ns->exists("wrapR"))
            {
                wrapR = parseTextureWrapMode(ns->getString("wrapR"), Texture::REPEAT);
            }
            Texture::Filter minFilter = parseTextureFilterMode(ns->getString("minFilter"), mipmap ? Texture::NEAREST_MIPMAP_LINEAR : Texture::LINEAR);
            Texture::Filter magFilter = parseTextureFilterMode(ns->getString("magFilter"), Texture::LINEAR);

            // Set the sampler parameter.
            GP_ASSERT(renderState->getParameter(name));
            Texture::Sampler* sampler = renderState->getParameter(name)->setValue(path.c_str(), mipmap);
            if (sampler)
            {
                sampler->setWrapMode(wrapS, wrapT, wrapR);
                sampler->setFilterMode(minFilter, magFilter);
            }
        }
        else if (strcmp(ns->getNamespace(), "renderState") == 0)
        {
            while ((name = ns->getNextProperty()))
            {
                GP_ASSERT(renderState->getStateBlock());
                renderState->getStateBlock()->setState(name, ns->getString());
            }
        }
    }
}

}
