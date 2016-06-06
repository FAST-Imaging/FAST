#define _USE_MATH_DEFINES

#include <GL/glew.h>
#include "VolumeRenderer.hpp"
#include "FAST/Data/Image.hpp"
#include "FAST/Utility.hpp"
#include "FAST/DeviceManager.hpp"
#include "FAST/Visualization/View.hpp"
#include "FAST/SceneGraph.hpp"
#include <QCursor>
#include "ColorTransferFunction.hpp"
#include "OpacityTransferFunction.hpp"
#include <boost/thread/lock_guard.hpp>


namespace fast {

void VolumeRenderer::resize(GLuint height, GLuint width){

	pboHasChanged = true;
	mIsModified = true;

}
void VolumeRenderer::setProjectionParameters(float fov, float aspect, float nearPlane, float farPlane, int width, int height){
	mWidth = width;
	mHeight = height;
	zNear = nearPlane;
	zFar = farPlane;
	topOfViewPlane=fabs(zNear)*tan(M_PI*fov/360);
	rightOfViewPlane=topOfViewPlane*aspect;

	projectionMatrix10 = (zFar+zNear)/(zFar-zNear);
	projectionMatrix14= (-2.0*zFar*zNear) / (zFar-zNear);

	pboHasChanged = true;

	mIsModified = true;
}
void VolumeRenderer::addInputConnection(ProcessObjectPort port) {

	

	if(numberOfVolumes<0)
        throw Exception("Not a correct number of volumes is given to VolumeRenderer");
	if(numberOfVolumes<maxNumberOfVolumes)
	{
		uint nr = getNrOfInputData();
		if(nr > 0)
		    createInputPort<Image>(nr);
	//	releaseInputAfterExecute(nr, false);
		setInputConnection(nr, port);
		//mInputs.push_back(getStaticInputData<Image>(0));
		
		//addParent(mInputs[numberOfVolumes]);
		numberOfVolumes++;
		mIsModified = true;
		mInputIsModified=true;
	}
	else
		printf("\n Warning: Volume Renderer currently supports only up to %d volumes. Extera inputs are denied. \n", maxNumberOfVolumes);
	
}
void VolumeRenderer::setOpacityTransferFunction(int volumeIndex, OpacityTransferFunction::pointer otf) {

	if (volumeIndex>=maxNumberOfVolumes)
		throw Exception("\nError: The volumeIndex for OpacityTransferFunction is out of range.");

	double xMin = otf->getXMin();
	double xMax = otf->getXMax();
	unsigned int XDef = static_cast<unsigned int>(xMax - xMin);

	if(((OpenCLDevice::pointer)getMainDevice())->isImageFormatSupported(CL_A, CL_FLOAT, CL_MEM_OBJECT_IMAGE2D)) {
        opacityFunc=(float *)(malloc(sizeof(float)*XDef));

        for (unsigned int c=0; c<otf->v.size()-1; c++)
        {
                int   S=otf->v[c+0].X;
                int   E=otf->v[c+1].X;
                float A1=otf->v[c].A;
                float A= (otf->v[c+1].A) - A1;
                float D=E-S;

                unsigned int index=0;
                for(unsigned int i=S-xMin; i<E-xMin; i++, index++)
                {
                        opacityFunc[i]=A1+A*index/D;//A

                }
        }
        d_opacityFuncArray[volumeIndex]=cl::Image2D(clContext, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR, cl::ImageFormat(CL_A, CL_FLOAT), XDef, 1, 0, opacityFunc, 0);
	} else {
		// Single channel images is not support on all platforms (e.g. Mac), thus use regular 4 channel images if it is not supported
        opacityFunc=(float *)(malloc(sizeof(float)*XDef*4));

        for (unsigned int c=0; c<otf->v.size()-1; c++)
        {
                int   S=otf->v[c+0].X;
                int   E=otf->v[c+1].X;
                float A1=otf->v[c].A;
                float A= (otf->v[c+1].A) - A1;
                float D=E-S;

                unsigned int index=0;
                for(unsigned int i=S-xMin; i<E-xMin; i++, index++)
                {
                        opacityFunc[i*4+3]=A1+A*index/D;//A

                }
        }
        d_opacityFuncArray[volumeIndex]=cl::Image2D(clContext, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR, cl::ImageFormat(CL_RGBA, CL_FLOAT), XDef, 1, 0, opacityFunc, 0);
	}
	opacityFuncDefs[volumeIndex] = XDef;
	opacityFuncMins[volumeIndex] = xMin;
	
	mIsModified = true;
}
void VolumeRenderer::setColorTransferFunction(int volumeIndex, ColorTransferFunction::pointer ctf) {

	if (volumeIndex >= maxNumberOfVolumes)
		throw Exception("\nError: The volumeIndex for ColorTransferFunction is out of range.");

	double xMin = ctf->getXMin();
	double xMax = ctf->getXMax();
	unsigned int XDef = static_cast<unsigned int>(xMax - xMin);

	transferFunc=(float *)(malloc(sizeof(float)*4*XDef));
	
	for (unsigned int c=0; c<ctf->v.size()-1; c++)
	{
		int   S=ctf->v[c+0].X;
		int   E=ctf->v[c+1].X;
		float R1=ctf->v[c].R;
		float G1=ctf->v[c].G;
		float B1=ctf->v[c].B;
		float R= (ctf->v[c+1].R) - R1;
		float G= (ctf->v[c+1].G) - G1;
		float B= (ctf->v[c+1].B) - B1;
		float D=E-S;

		unsigned int index=0;
		for (unsigned int i = S - xMin; i<E - xMin; i++, index++)
		{
			transferFunc[i*4+0]=R1+R*index/D;//R
			transferFunc[i*4+1]=G1+G*index/D;//G
			transferFunc[i*4+2]=B1+B*index/D;//B
			transferFunc[i*4+3]=1.0f;//A
		}
	}

	d_transferFuncArray[volumeIndex]=cl::Image2D(clContext, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR, cl::ImageFormat(CL_RGBA, CL_FLOAT), XDef, 1, 0, transferFunc, 0);
	colorFuncDefs[volumeIndex] = XDef;
	colorFuncMins[volumeIndex] = xMin;
	mIsModified = true;
}
void VolumeRenderer::addGeometryColorTexture(GLuint geoColorTex)
{
	if (geoColorTex)
		mGeoColorTex = geoColorTex;
}

void VolumeRenderer::addGeometryDepthTexture(GLuint geoDepthTex)
{
	if (geoDepthTex)
		mGeoDepthTex = geoDepthTex;
}
void VolumeRenderer::turnOffTransformations() {
    mDoTransformations = false;
}
//this returns the boundingbox of ALL volumes
BoundingBox VolumeRenderer::getBoundingBox()
{
	std::vector<Vector3f> coordinates;
	for (uint i = 0; i < getNrOfInputData(); i++) {
		BoundingBox transformedBoundingBox = mImagesToRender[i]->getTransformedBoundingBox();
		MatrixXf corners = transformedBoundingBox.getCorners();
		for (uint j = 0; j < 8; j++) {
			coordinates.push_back((Vector3f)corners.row(j));
		}
	}
	return BoundingBox(coordinates);
}
void VolumeRenderer::setUserTransform(int volumeIndex, const float userTransform[16]){
	
	for(int i=0; i<16; i++)
		mUserTransforms[volumeIndex*i]=userTransform[i];

	switch (volumeIndex)
	{
	case 0: 
		for(int i=0; i<16; i++)
			mUserTransform0[i]=userTransform[i];
		break;
	case 1: 
		for(int i=0; i<16; i++)
			mUserTransform1[i]=userTransform[i]; 
		break;
	case 2: 
		for(int i=0; i<16; i++)
			mUserTransform2[i]=userTransform[i]; 
		break;
	case 3: 
		for(int i=0; i<16; i++)
			mUserTransform3[i]=userTransform[i]; 
		break;
	case 4: 
		for(int i=0; i<16; i++)
			mUserTransform4[i]=userTransform[i]; 
		break;

	}

	doUserTransforms[volumeIndex]=true;
	
}
VolumeRenderer::VolumeRenderer() : Renderer() {
    createInputPort<Image>(0, false);

    mDevice = DeviceManager::getInstance().getDefaultVisualizationDevice();
	clContext = mDevice->getContext();

	mInputIsModified = true;
	mIsModified = true;
	mDoTransformations = true;
	mOutputIsCreated=false;

	numberOfVolumes=0;

//	inputs.clear();
	//Default window size
	mHeight = 512;
	mWidth = 512;

	d_invViewMatrices= cl::Buffer(clContext, CL_MEM_READ_WRITE, maxNumberOfVolumes*16*sizeof(float));
	d_boxMaxs = cl::Buffer(clContext, CL_MEM_READ_WRITE, maxNumberOfVolumes * 3 * sizeof(float));
	d_opacityFuncDefs = cl::Buffer(clContext, CL_MEM_READ_WRITE, maxNumberOfVolumes * sizeof(float));
	d_opacityFuncMins = cl::Buffer(clContext, CL_MEM_READ_WRITE, maxNumberOfVolumes * sizeof(float));
	d_colorFuncDefs = cl::Buffer(clContext, CL_MEM_READ_WRITE, maxNumberOfVolumes * sizeof(float));
	d_colorFuncMins = cl::Buffer(clContext, CL_MEM_READ_WRITE, maxNumberOfVolumes * sizeof(float));

	includeGeometry=false;

	pboHasChanged = true;
	pbo=0;

	for (int i=0; i<maxNumberOfVolumes; i++)
		doUserTransforms[i]=false;

}
void VolumeRenderer::setIncludeGeometry(bool p){
	
	includeGeometry=p;
	mInputIsModified = true;
}
void VolumeRenderer::setModelViewMatrix(GLfloat mView[16]){

	for (int i = 0; i < 16; i++)
		modelView[i] = mView[i];
	mIsModified = true;
}
void VolumeRenderer::execute() {


	boost::lock_guard<boost::mutex> lock(mMutex);

	mOutputIsCreated = false;

	numberOfVolumes = getNrOfInputData();
	if (numberOfVolumes<0)
		throw Exception("Not a correct number of volumes is given to VolumeRenderer.");
	if (numberOfVolumes>maxNumberOfVolumes)
		printf("Warning: Volume Renderer currently supports only up to %d volumes. Extera inputs are denied. \n", maxNumberOfVolumes);
	
	// This simply gets the input data for each connection and puts it into a data structure
	for (unsigned int inputNr = 0; inputNr < numberOfVolumes; inputNr++)
	{

		Image::pointer input = getStaticInputData<Image>(inputNr);
		mImagesToRender[inputNr] = input;
		//inputs.push_back(input);


		if (mImagesToRender[inputNr]->getDimensions() != 3)
		{
			char errorMessage[255];
			sprintf(errorMessage, "The VolumeRenderer only supports 3D images; check input number %d.", inputNr);
			throw Exception(errorMessage);
		}

	}


	//glEnable(GL_NORMALIZE);
	//glEnable(GL_DEPTH_TEST);

	//-------------------------------

	

	glPushMatrix();

	for (int inputNr = 0; inputNr < numberOfVolumes; inputNr++)
	{
		Image::pointer input = mImagesToRender[inputNr];//getStaticInputData<Image>(inputNr);

		glLoadIdentity();
		glMultMatrixf(modelView);

		if (mDoTransformations)
		{
			AffineTransformation::pointer transform = SceneGraph::getAffineTransformationFromData(input);
			transform->scale(input->getSpacing());

			glMultMatrixf(transform->data());
		}


		if (doUserTransforms[inputNr])
			switch (inputNr)
		{
			case 0: glMultTransposeMatrixf(mUserTransform0); break;
			case 1: glMultTransposeMatrixf(mUserTransform1); break;
			case 2: glMultTransposeMatrixf(mUserTransform2); break;
			case 3: glMultTransposeMatrixf(mUserTransform3); break;
			case 4: glMultTransposeMatrixf(mUserTransform4); break;
		}


		GLfloat modelViewMatrix[16];
		glGetFloatv(GL_MODELVIEW_MATRIX, modelViewMatrix);
		switch (inputNr)
		{
		case 0:
			gluInvertMatrix(modelViewMatrix, invViewMatrix0);
			for (uint i = 0; i < 16; i++)
				invViewMatrices[i] = invViewMatrix0[i];
			break;
		case 1:
			gluInvertMatrix(modelViewMatrix, invViewMatrix1);
			for (uint i = 0; i < 16; i++)
				invViewMatrices[i + 16] = invViewMatrix1[i];
			break;
		case 2:
			gluInvertMatrix(modelViewMatrix, invViewMatrix2);
			for (uint i = 0; i < 16; i++)
				invViewMatrices[i + 32] = invViewMatrix2[i];
			break;
		case 3:
			gluInvertMatrix(modelViewMatrix, invViewMatrix3);
			for (uint i = 0; i < 16; i++)
				invViewMatrices[i + 48] = invViewMatrix3[i];
			break;
		case 4:
			gluInvertMatrix(modelViewMatrix, invViewMatrix4);
			for (uint i = 0; i < 16; i++)
				invViewMatrices[i + 64] = invViewMatrix4[i];
			break;
		}
	}

	glPopMatrix();



	if (mInputIsModified)
	{
		// Compile program
		char buffer[128];
		sprintf(buffer, "-cl-fast-relaxed-math -D VOL%d -D numberOfVolumes=%d", numberOfVolumes, numberOfVolumes);
		for (unsigned int i = 0; i < numberOfVolumes; i++)
		{
			Image::pointer input = mImagesToRender[i];//getStaticInputData<Image>(i);
			char dataTypeBuffer[128];
			unsigned int volumeDataType = input->getDataType();

			if (volumeDataType == fast::TYPE_FLOAT)
				sprintf(dataTypeBuffer, " -D TYPE_FLOAT%d", i + 1);
			else
			{
				if ((volumeDataType == fast::TYPE_UINT8) || (volumeDataType == fast::TYPE_UINT16))
					sprintf(dataTypeBuffer, " -D TYPE_UINT%d", i + 1);
				else
					sprintf(dataTypeBuffer, " -D TYPE_INT%d", i + 1);
			}
			strcat(buffer, dataTypeBuffer);

			boxMaxs[i * 3 + 0] = input->getWidth();
			boxMaxs[i * 3 + 1] = input->getHeight();
			boxMaxs[i * 3 + 2] = input->getDepth();

		}

		std::string str(buffer);
		int programNr;
		if (includeGeometry)
			programNr = mDevice->createProgramFromSource(std::string(FAST_SOURCE_DIR) + "/Visualization/VolumeRenderer/VolumeRendererWithGeo.cl", str);
		else
			programNr = mDevice->createProgramFromSource(std::string(FAST_SOURCE_DIR) + "/Visualization/VolumeRenderer/VolumeRendererNoGeo.cl", str);
		program = mDevice->getProgram(programNr);
		renderKernel = cl::Kernel(program, "d_render");



		mInputIsModified = false;
	}

	if (pboHasChanged)
	{
		if (pbo)
		{
			// delete old buffer
			glDeleteBuffers(1, &pbo);
		}
		// create pixel buffer object for display
		glGenBuffersARB(1, &pbo);
		glBindBufferARB(GL_PIXEL_UNPACK_BUFFER_ARB, pbo);
		glBufferDataARB(GL_PIXEL_UNPACK_BUFFER_ARB, mHeight * mWidth * 4, 0, GL_STREAM_DRAW_ARB);
		glBindBufferARB(GL_PIXEL_UNPACK_BUFFER_ARB, 0);

		// Create CL-GL image
		pbo_cl = cl::BufferGL(clContext, CL_MEM_WRITE_ONLY, pbo);

		pboHasChanged = false;
	}

	if (!includeGeometry)
	{

		float density = 1.0f;
		float brightness = 1.0f;

		OpenCLImageAccess::pointer access = mImagesToRender[0]->getOpenCLImageAccess(ACCESS_READ, mDevice);//getStaticInputData<Image>(0)->getOpenCLImageAccess3D(ACCESS_READ, mDevice);
		cl::Image3D* clImage = access->get3DImage();

		renderKernel.setArg(0, pbo_cl);
		renderKernel.setArg(1, mWidth);
		renderKernel.setArg(2, mHeight);
		renderKernel.setArg(3, density);
		renderKernel.setArg(4, brightness);
		renderKernel.setArg(5, zNear);
		renderKernel.setArg(6, zFar);
		renderKernel.setArg(7, topOfViewPlane);
		renderKernel.setArg(8, rightOfViewPlane);
		renderKernel.setArg(9, projectionMatrix10);
		renderKernel.setArg(10, projectionMatrix14);
		renderKernel.setArg(11, d_invViewMatrices);
		renderKernel.setArg(12, *clImage);
		renderKernel.setArg(13, d_transferFuncArray[0]);
		renderKernel.setArg(14, d_opacityFuncArray[0]);
		renderKernel.setArg(15, d_boxMaxs);
		renderKernel.setArg(16, d_opacityFuncDefs);
		renderKernel.setArg(17, d_opacityFuncMins);
		renderKernel.setArg(18, d_colorFuncDefs);
		renderKernel.setArg(19, d_colorFuncMins);
		if (numberOfVolumes > 1)
		{
			OpenCLImageAccess::pointer access2 = mImagesToRender[1]->getOpenCLImageAccess(ACCESS_READ, mDevice);
			cl::Image3D* clImage2 = access2->get3DImage();
			renderKernel.setArg(20, *clImage2);
			renderKernel.setArg(21, d_transferFuncArray[1]);
			renderKernel.setArg(22, d_opacityFuncArray[1]);

			if (numberOfVolumes > 2)
			{
				OpenCLImageAccess::pointer access3 = mImagesToRender[2]->getOpenCLImageAccess(ACCESS_READ, mDevice);//getStaticInputData<Image>(2)->getOpenCLImageAccess3D(ACCESS_READ, mDevice);
				cl::Image3D* clImage3 = access3->get3DImage();
				renderKernel.setArg(23, *clImage3);
				renderKernel.setArg(24, d_transferFuncArray[2]);
				renderKernel.setArg(25, d_opacityFuncArray[2]);

				if (numberOfVolumes > 3)
				{
					OpenCLImageAccess::pointer access4 = mImagesToRender[3]->getOpenCLImageAccess(ACCESS_READ, mDevice);//getStaticInputData<Image>(3)->getOpenCLImageAccess3D(ACCESS_READ, mDevice);
					cl::Image3D* clImage4 = access4->get3DImage();
					renderKernel.setArg(26, *clImage4);
					renderKernel.setArg(27, d_transferFuncArray[3]);
					renderKernel.setArg(28, d_opacityFuncArray[3]);

					if (numberOfVolumes > 4)
					{
						OpenCLImageAccess::pointer access5 = mImagesToRender[4]->getOpenCLImageAccess(ACCESS_READ, mDevice);//getStaticInputData<Image>(4)->getOpenCLImageAccess3D(ACCESS_READ, mDevice);
						cl::Image3D* clImage5 = access5->get3DImage();
						renderKernel.setArg(29, *clImage5);
						renderKernel.setArg(30, d_transferFuncArray[4]);
						renderKernel.setArg(31, d_opacityFuncArray[4]);

					}
				}

			}

		}

		std::vector<cl::Memory> v;
		v.push_back(pbo_cl);


		mDevice->getCommandQueue().enqueueAcquireGLObjects(&v);
		mDevice->getCommandQueue().enqueueWriteBuffer(d_invViewMatrices, CL_FALSE, 0, sizeof(invViewMatrices), invViewMatrices);
		mDevice->getCommandQueue().enqueueWriteBuffer(d_boxMaxs, CL_FALSE, 0, sizeof(boxMaxs), boxMaxs);
		mDevice->getCommandQueue().enqueueWriteBuffer(d_opacityFuncDefs, CL_FALSE, 0, sizeof(opacityFuncDefs), opacityFuncDefs);
		mDevice->getCommandQueue().enqueueWriteBuffer(d_opacityFuncMins, CL_FALSE, 0, sizeof(opacityFuncMins), opacityFuncMins);
		mDevice->getCommandQueue().enqueueWriteBuffer(d_colorFuncDefs, CL_FALSE, 0, sizeof(colorFuncDefs), colorFuncDefs);
		mDevice->getCommandQueue().enqueueWriteBuffer(d_colorFuncMins, CL_FALSE, 0, sizeof(colorFuncMins), colorFuncMins);

		try
		{
			mDevice->getCommandQueue().enqueueNDRangeKernel(
				renderKernel,
				cl::NullRange,
				cl::NDRange(mWidth, mHeight),
				cl::NullRange
				);
		}
		catch (Exception e)
		{
			return;
		}
		mDevice->getCommandQueue().enqueueReleaseGLObjects(&v);
		mDevice->getCommandQueue().finish();
	}

	mOutputIsCreated = true;

}

void VolumeRenderer::draw() {
	
	boost::lock_guard<boost::mutex> lock(mMutex);
	
	if (!mOutputIsCreated)
		return;

	if (includeGeometry)
	{

#if defined(CL_VERSION_1_2)
		mImageGLGeoColor = cl::ImageGL(clContext, CL_MEM_READ_ONLY, GL_TEXTURE_2D, 0, mGeoColorTex);
#else
		mImageGLGeoColor = cl::Image2DGL(clContext, CL_MEM_READ_ONLY, GL_TEXTURE_2D, 0, mGeoColorTex);
#endif
#if defined(CL_VERSION_1_2)
		mImageGLGeoDepth = cl::ImageGL(clContext, CL_MEM_READ_ONLY, GL_TEXTURE_2D, 0, mGeoDepthTex);
#else
		mImageGLGeoDepth = cl::Image2DGL(clContext, CL_MEM_READ_ONLY, GL_TEXTURE_2D, 0, mGeoDepthTex);
#endif
		float density = 1.0f;
		float brightness = 1.0f;

		OpenCLImageAccess::pointer access = mImagesToRender[0]->getOpenCLImageAccess(ACCESS_READ, mDevice);//getStaticInputData<Image>(0)->getOpenCLImageAccess3D(ACCESS_READ, mDevice);

		cl::Image3D* clImage = access->get3DImage();

		renderKernel.setArg(0, pbo_cl);
		renderKernel.setArg(1, mWidth);
		renderKernel.setArg(2, mHeight);
		renderKernel.setArg(3, density);
		renderKernel.setArg(4, brightness);
		renderKernel.setArg(5, zNear);
		renderKernel.setArg(6, zFar);
		renderKernel.setArg(7, topOfViewPlane);
		renderKernel.setArg(8, rightOfViewPlane);
		renderKernel.setArg(9, projectionMatrix10);
		renderKernel.setArg(10, projectionMatrix14);
		renderKernel.setArg(11, d_invViewMatrices);
		renderKernel.setArg(12, *clImage);
		renderKernel.setArg(13, d_transferFuncArray[0]);
		renderKernel.setArg(14, d_opacityFuncArray[0]);
		renderKernel.setArg(15, mImageGLGeoColor);
		renderKernel.setArg(16, mImageGLGeoDepth);
		renderKernel.setArg(17, d_boxMaxs);
		renderKernel.setArg(18, d_opacityFuncDefs);
		renderKernel.setArg(19, d_opacityFuncMins);
		renderKernel.setArg(20, d_colorFuncDefs);
		renderKernel.setArg(21, d_colorFuncMins);
		if (numberOfVolumes > 1)
		{
			OpenCLImageAccess::pointer access2 = mImagesToRender[1]->getOpenCLImageAccess(ACCESS_READ, mDevice);//getStaticInputData<Image>(1)->getOpenCLImageAccess3D(ACCESS_READ, mDevice);
			cl::Image3D* clImage2 = access2->get3DImage();
			renderKernel.setArg(22, *clImage2);
			renderKernel.setArg(23, d_transferFuncArray[1]);
			renderKernel.setArg(24, d_opacityFuncArray[1]);

			if (numberOfVolumes > 2)
			{
				OpenCLImageAccess::pointer access3 = mImagesToRender[2]->getOpenCLImageAccess(ACCESS_READ, mDevice);//getStaticInputData<Image>(2)->getOpenCLImageAccess3D(ACCESS_READ, mDevice);
				cl::Image3D* clImage3 = access3->get3DImage();
				renderKernel.setArg(25, *clImage3);
				renderKernel.setArg(26, d_transferFuncArray[2]);
				renderKernel.setArg(27, d_opacityFuncArray[2]);

				if (numberOfVolumes > 3)
				{
					OpenCLImageAccess::pointer access4 = mImagesToRender[3]->getOpenCLImageAccess(ACCESS_READ, mDevice);//getStaticInputData<Image>(3)->getOpenCLImageAccess3D(ACCESS_READ, mDevice);
					cl::Image3D* clImage4 = access4->get3DImage();
					renderKernel.setArg(28, *clImage4);
					renderKernel.setArg(29, d_transferFuncArray[3]);
					renderKernel.setArg(30, d_opacityFuncArray[3]);

					if (numberOfVolumes > 4)
					{
						OpenCLImageAccess::pointer access5 = mImagesToRender[4]->getOpenCLImageAccess(ACCESS_READ, mDevice);//getStaticInputData<Image>(4)->getOpenCLImageAccess3D(ACCESS_READ, mDevice);
						cl::Image3D* clImage5 = access5->get3DImage();
						renderKernel.setArg(31, *clImage5);
						renderKernel.setArg(32, d_transferFuncArray[4]);
						renderKernel.setArg(33, d_opacityFuncArray[4]);
					}
				}

			}

		}
		std::vector<cl::Memory> v;
		v.push_back(pbo_cl);

		v.push_back(mImageGLGeoColor);
		v.push_back(mImageGLGeoDepth);

		mDevice->getCommandQueue().enqueueAcquireGLObjects(&v);
		mDevice->getCommandQueue().enqueueWriteBuffer(d_invViewMatrices, CL_FALSE, 0, sizeof(invViewMatrices), invViewMatrices);
		mDevice->getCommandQueue().enqueueWriteBuffer(d_boxMaxs, CL_FALSE, 0, sizeof(boxMaxs), boxMaxs);
		mDevice->getCommandQueue().enqueueWriteBuffer(d_opacityFuncDefs, CL_FALSE, 0, sizeof(opacityFuncDefs), opacityFuncDefs);
		mDevice->getCommandQueue().enqueueWriteBuffer(d_opacityFuncMins, CL_FALSE, 0, sizeof(opacityFuncMins), opacityFuncMins);
		mDevice->getCommandQueue().enqueueWriteBuffer(d_colorFuncDefs, CL_FALSE, 0, sizeof(colorFuncDefs), colorFuncDefs);
		mDevice->getCommandQueue().enqueueWriteBuffer(d_colorFuncMins, CL_FALSE, 0, sizeof(colorFuncMins), colorFuncMins);

		try
		{
			mDevice->getCommandQueue().enqueueNDRangeKernel(
				renderKernel,
				cl::NullRange,
				cl::NDRange(mWidth, mHeight),
				cl::NullRange
				);
		}
		catch (Exception e)
		{

		}
		mDevice->getCommandQueue().enqueueReleaseGLObjects(&v);
		mDevice->getCommandQueue().finish();

	}	//includeGeometry

	glEnable(GL_DEPTH_TEST);
	glEnable(GL_TEXTURE_2D);

	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	glViewport(0, 0, mWidth, mHeight);
	glOrtho(0, mWidth, 0, mHeight, 0, 512);
	// draw image from PBO
	glClear(GL_COLOR_BUFFER_BIT);
	glDisable(GL_DEPTH_TEST);
	glDisable(GL_TEXTURE_2D);
	glRasterPos2i(0, 0);
	glBindBufferARB(GL_PIXEL_UNPACK_BUFFER_ARB, pbo);
	glDrawPixels(mWidth, mHeight, GL_RGBA, GL_UNSIGNED_INT_8_8_8_8, 0);
	glBindBufferARB(GL_PIXEL_UNPACK_BUFFER_ARB, 0);
	//glFinish();
	
}


void VolumeRenderer::mouseEvents() 
{
	mIsModified = true;
}
bool VolumeRenderer::gluInvertMatrix(const float m[16], float invOut[16])
{
    float inv[16], det;
    int i;

    inv[0] = m[5]  * m[10] * m[15] - 
             m[5]  * m[11] * m[14] - 
             m[9]  * m[6]  * m[15] + 
             m[9]  * m[7]  * m[14] +
             m[13] * m[6]  * m[11] - 
             m[13] * m[7]  * m[10];

    inv[4] = -m[4]  * m[10] * m[15] + 
              m[4]  * m[11] * m[14] + 
              m[8]  * m[6]  * m[15] - 
              m[8]  * m[7]  * m[14] - 
              m[12] * m[6]  * m[11] + 
              m[12] * m[7]  * m[10];

    inv[8] = m[4]  * m[9] * m[15] - 
             m[4]  * m[11] * m[13] - 
             m[8]  * m[5] * m[15] + 
             m[8]  * m[7] * m[13] + 
             m[12] * m[5] * m[11] - 
             m[12] * m[7] * m[9];

    inv[12] = -m[4]  * m[9] * m[14] + 
               m[4]  * m[10] * m[13] +
               m[8]  * m[5] * m[14] - 
               m[8]  * m[6] * m[13] - 
               m[12] * m[5] * m[10] + 
               m[12] * m[6] * m[9];

    inv[1] = -m[1]  * m[10] * m[15] + 
              m[1]  * m[11] * m[14] + 
              m[9]  * m[2] * m[15] - 
              m[9]  * m[3] * m[14] - 
              m[13] * m[2] * m[11] + 
              m[13] * m[3] * m[10];

    inv[5] = m[0]  * m[10] * m[15] - 
             m[0]  * m[11] * m[14] - 
             m[8]  * m[2] * m[15] + 
             m[8]  * m[3] * m[14] + 
             m[12] * m[2] * m[11] - 
             m[12] * m[3] * m[10];

    inv[9] = -m[0]  * m[9] * m[15] + 
              m[0]  * m[11] * m[13] + 
              m[8]  * m[1] * m[15] - 
              m[8]  * m[3] * m[13] - 
              m[12] * m[1] * m[11] + 
              m[12] * m[3] * m[9];

    inv[13] = m[0]  * m[9] * m[14] - 
              m[0]  * m[10] * m[13] - 
              m[8]  * m[1] * m[14] + 
              m[8]  * m[2] * m[13] + 
              m[12] * m[1] * m[10] - 
              m[12] * m[2] * m[9];

    inv[2] = m[1]  * m[6] * m[15] - 
             m[1]  * m[7] * m[14] - 
             m[5]  * m[2] * m[15] + 
             m[5]  * m[3] * m[14] + 
             m[13] * m[2] * m[7] - 
             m[13] * m[3] * m[6];

    inv[6] = -m[0]  * m[6] * m[15] + 
              m[0]  * m[7] * m[14] + 
              m[4]  * m[2] * m[15] - 
              m[4]  * m[3] * m[14] - 
              m[12] * m[2] * m[7] + 
              m[12] * m[3] * m[6];

    inv[10] = m[0]  * m[5] * m[15] - 
              m[0]  * m[7] * m[13] - 
              m[4]  * m[1] * m[15] + 
              m[4]  * m[3] * m[13] + 
              m[12] * m[1] * m[7] - 
              m[12] * m[3] * m[5];

    inv[14] = -m[0]  * m[5] * m[14] + 
               m[0]  * m[6] * m[13] + 
               m[4]  * m[1] * m[14] - 
               m[4]  * m[2] * m[13] - 
               m[12] * m[1] * m[6] + 
               m[12] * m[2] * m[5];

    inv[3] = -m[1] * m[6] * m[11] + 
              m[1] * m[7] * m[10] + 
              m[5] * m[2] * m[11] - 
              m[5] * m[3] * m[10] - 
              m[9] * m[2] * m[7] + 
              m[9] * m[3] * m[6];

    inv[7] = m[0] * m[6] * m[11] - 
             m[0] * m[7] * m[10] - 
             m[4] * m[2] * m[11] + 
             m[4] * m[3] * m[10] + 
             m[8] * m[2] * m[7] - 
             m[8] * m[3] * m[6];

    inv[11] = -m[0] * m[5] * m[11] + 
               m[0] * m[7] * m[9] + 
               m[4] * m[1] * m[11] - 
               m[4] * m[3] * m[9] - 
               m[8] * m[1] * m[7] + 
               m[8] * m[3] * m[5];

    inv[15] = m[0] * m[5] * m[10] - 
              m[0] * m[6] * m[9] - 
              m[4] * m[1] * m[10] + 
              m[4] * m[2] * m[9] + 
              m[8] * m[1] * m[6] - 
              m[8] * m[2] * m[5];

    det = m[0] * inv[0] + m[1] * inv[4] + m[2] * inv[8] + m[3] * inv[12];

    if (det == 0)
        return false;

    det = 1.0 / det;

    for (i = 0; i < 16; i++)
        invOut[i] = inv[i] * det;

    return true;
}


} // namespace fast

