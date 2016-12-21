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


Material::Material()
{
}

Material::~Material()
{

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

Material* Material::create(const char* vshPath, const char* fshPath, const char* defines)
{
    GP_ASSERT(vshPath);
    GP_ASSERT(fshPath);

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


    Material* material = new Material();

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
	shaderSource += (vshPath ? vshSourceStr.c_str() : vshSource);

	material->shaderStages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	material->shaderStages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;

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
	material->shaderStages[0].module = shaderModule;
	material->shaderStages[0].pName = "main"; // todo : make param
	material->shaderModules.push_back(material->shaderStages[0].module);

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
	material->shaderStages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	material->shaderStages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;

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

	material->shaderStages[1].module = shaderModuleFra;
	material->shaderStages[1].pName = "main"; // todo : make param
	material->shaderModules.push_back(material->shaderStages[1].module);

	material->createPipelineLayout();

    return material;
}







void Material::setNodeBinding(Node* node)
{
    RenderState::setNodeBinding(node);
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
