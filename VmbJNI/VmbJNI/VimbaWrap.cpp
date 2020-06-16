#include <iostream>

#include "Bitmap.h"
using namespace std;
//#include "stdafx.h"
#include "VimbaWrap.h"
#include "VimbaC.h"
#include "VmbCommonTypes.h"

#pragma comment(lib,"lib\\VimbaC.lib")
/*
 * Class:     VimbaWrap
 * Method:    Initialize
 * Signature: ()V
 */
const char* pCameraID;
VmbHandle_t         cameraHandle        = NULL; 


inline VmbError_t initCamera(const char* cameraid)
{
    VmbError_t err;
	VmbUint32_t         camCount = 0;
	VmbUint32_t         nFoundCount = 0;
	VmbCameraInfo_t     *pCameras           = NULL;
	VmbAccessMode_t     cameraAccessMode    = VmbAccessModeFull;// We open the camera with full access
    // VmbHandle_t         cameraHandle        = NULL;             // A handle to our camera
    VmbBool_t           bIsCommandDone      = VmbBoolFalse;     // Has a command finished execution
	//const char* pCameraID;

	err = VmbStartup();  //Startup System
	if (err != VmbErrorSuccess) return err;

	err = VmbFeatureCommandRun(gVimbaHandle, "GeVDiscoveryAllOnce");
	if (err != VmbErrorSuccess) return err;

	err = VmbCamerasList(NULL, 0, &camCount, sizeof *pCameras); //Retrieve a list of all cameras.
	if (err != VmbErrorSuccess) return err;

	pCameras = (VmbCameraInfo_t*)malloc( camCount * sizeof( *pCameras ));

	if( camCount != 0)
    {   
		err = VmbCamerasList( pCameras, camCount, &nFoundCount, sizeof *pCameras );
		if( nFoundCount != 0 && err == VmbErrorSuccess)
        //pCameraID = pCameras[0].cameraIdString;

		 pCameraID = cameraid;
    }
    else
    {
        pCameraID = NULL;
        err = VmbErrorNotFound;
        //printf( "Camera lost.\n" );
    }
	 free( pCameras );
     pCameras = NULL;

	  if ( NULL != pCameraID )//打开相机
        {
            // Open camera
            err = VmbCameraOpen( pCameraID, cameraAccessMode, &cameraHandle );
            if ( VmbErrorSuccess == err )
            {
                // printf( "Camera ID: %s\n\n", pCameraID );
                // Set the GeV packet size to the highest possible value
                // (In this example we do not test whether this cam actually is a GigE cam)
                if ( VmbErrorSuccess == VmbFeatureCommandRun( cameraHandle, "GVSPAdjustPacketSize" ))
                {
                    do
                    {
                        if ( VmbErrorSuccess != VmbFeatureCommandIsDone(cameraHandle,"GVSPAdjustPacketSize",&bIsCommandDone ))
                        {
                            break;
                        }
                    } while ( VmbBoolFalse == bIsCommandDone );
                }
			}
	  }


	return err;
}
inline VmbError_t AcquireSingle(int timeout,const char* pFileName)
{
	VmbError_t err;
	VmbFrame_t          frame;                                  // The frame we capture
    const char*         pPixelFormat        = NULL;             // The pixel format we use for acquisition
    VmbInt64_t          nPayloadSize        = 0;      
	AVTBitmap           bitmap;  

    if ( cameraHandle != NULL )
    {
		 err = VmbFeatureEnumSet( cameraHandle, "PixelFormat", "RGB8Packed" );
        
		 if ( VmbErrorSuccess != err )
        {
            // Fall back to Mono
            err = VmbFeatureEnumSet( cameraHandle, "PixelFormat", "Mono8" );
        }
        // Read back pixel format
        err = VmbFeatureEnumGet( cameraHandle, "PixelFormat", &pPixelFormat );
		if (err != VmbErrorSuccess) return err;
        // Evaluate frame size
        err = VmbFeatureIntGet( cameraHandle, "PayloadSize", &nPayloadSize );
        if ( VmbErrorSuccess == err )
        {
            frame.buffer        = (unsigned char*)malloc( (VmbUint32_t)nPayloadSize );
            frame.bufferSize    = (VmbUint32_t)nPayloadSize;

            // Announce Frame
            err = VmbFrameAnnounce( cameraHandle, &frame, (VmbUint32_t)sizeof( VmbFrame_t ));
            if ( VmbErrorSuccess == err )
            {
                // Start Capture Engine
                err = VmbCaptureStart( cameraHandle );
                if ( VmbErrorSuccess == err )
                {
                    // Queue Frame
                    err = VmbCaptureFrameQueue( cameraHandle, &frame, NULL );
                    if ( VmbErrorSuccess == err )
                    {
                        // Start Acquisition
                        err = VmbFeatureCommandRun( cameraHandle,"AcquisitionStart" );
                        if ( VmbErrorSuccess == err )
                        {
                            // Capture one frame synchronously
                            err = VmbCaptureFrameWait( cameraHandle, &frame, timeout );
                            if ( VmbErrorSuccess == err )
                            {
                                // Convert the captured frame to a bitmap and save to disk
                                if ( VmbFrameStatusComplete == frame.receiveStatus )
                                {
                                    bitmap.bufferSize = frame.imageSize;
                                    bitmap.width = frame.width;
                                    bitmap.height = frame.height;
                                    // We only support Mono and RGB in this example
                                    if ( 0 == strcmp( "RGB8Packed", pPixelFormat ))
                                    {
                                        bitmap.colorCode = ColorCodeRGB24;
                                    }
                                    else
                                    {
                                        bitmap.colorCode = ColorCodeMono8;
                                    }
                                    // Create the bitmap
                                    if ( 0 == AVTCreateBitmap( &bitmap, frame.buffer ))
                                    {
                                        printf( "Could not create bitmap.\n" );
                                    }
                                    else
                                    {
                                        // Save the bitmap
                                        if ( 0 == AVTWriteBitmapToFile( &bitmap, pFileName ))
                                        {
                                            printf( "Could not write bitmap to file.\n" );
                                        }
                                        else
                                        {
                                            printf( "Bitmap successfully written to file \"%s\"\n", pFileName );
                                            // Release the bitmap's buffer
                                            if ( 0 == AVTReleaseBitmap( &bitmap ))
                                            {
                                                printf( "Could not release the bitmap.\n" );
                                            }
                                        }
                                    }                                                                                                        
                                }
                                
                            }                           
                            // Stop Acquisition
                            err = VmbFeatureCommandRun( cameraHandle,"AcquisitionStop" );                                            
                        }                       
                    }                   
                    // Stop Capture Engine
                    err = VmbCaptureEnd( cameraHandle );                                    
                }              
                // Revoke frame
                err = VmbFrameRevoke( cameraHandle, &frame );                                
            }          
            free( frame.buffer );
            frame.buffer = NULL;
        }
    }
	return err;

}
inline bool releaseCamera()
{
	VmbError_t err;
	err = VmbCameraClose ( cameraHandle );
	VmbShutdown();

	cameraHandle = NULL; 
	pCameraID = NULL;

	if (err==VmbErrorSuccess)
		return true;
	else
		return false;
}



JNIEXPORT jboolean JNICALL Java_VimbaWrap_Initialize
  (JNIEnv *env, jobject obj, jstring cameraID)
{
	const char* str = env->GetStringUTFChars(cameraID, 0);
	if (initCamera(str)==VmbErrorSuccess)
		return true;
	else	
		return false;
}


JNIEXPORT jboolean JNICALL Java_VimbaWrap_Snap
  (JNIEnv *env, jobject obj, jint Timeout,jstring Imagelocation)
{
	const char* str = env->GetStringUTFChars(Imagelocation, 0);
	
	if (AcquireSingle(Timeout,str)==VmbErrorSuccess)
		return true;
	else	
		return false;

}

JNIEXPORT jboolean JNICALL Java_VimbaWrap_Realese
  (JNIEnv *env, jobject obj)
{
	bool tempvalue= releaseCamera();	
	return tempvalue;
	
}