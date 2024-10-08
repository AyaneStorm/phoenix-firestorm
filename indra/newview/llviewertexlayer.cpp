/**
 * @file llviewertexlayer.cpp
 * @brief Viewer texture layer. Used for avatars.
 *
 * $LicenseInfo:firstyear=2012&license=viewerlgpl$
 * Second Life Viewer Source Code
 * Copyright (C) 2010, Linden Research, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation;
 * version 2.1 of the License only.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * Linden Research, Inc., 945 Battery Street, San Francisco, CA  94111  USA
 * $/LicenseInfo$
 */

#include "llviewerprecompiledheaders.h"

#include "llviewertexlayer.h"

#include "llagent.h"
#include "llimagej2c.h"
#include "llnotificationsutil.h"
#include "llviewerregion.h"
#include "llglslshader.h"
#include "llvoavatarself.h"
#include "pipeline.h"
#include "llviewercontrol.h"

#include "llviewerassetupload.h"
#include "llsdutil.h"
#include "llfilesystem.h" // <FS:Ansariel> [Legacy Bake]

static const S32 BAKE_UPLOAD_ATTEMPTS = 7;
static const F32 BAKE_UPLOAD_RETRY_DELAY = 2.f; // actual delay grows by power of 2 each attempt

// runway consolidate
extern std::string self_av_string();

//-----------------------------------------------------------------------------
// LLViewerTexLayerSetBuffer
// The composite image that a LLViewerTexLayerSet writes to.  Each LLViewerTexLayerSet has one.
//-----------------------------------------------------------------------------

// static
S32 LLViewerTexLayerSetBuffer::sGLByteCount = 0;

LLViewerTexLayerSetBuffer::LLViewerTexLayerSetBuffer(LLTexLayerSet* const owner,
                                         S32 width, S32 height) :
    // ORDER_LAST => must render these after the hints are created.
    LLTexLayerSetBuffer(owner),
    LLViewerDynamicTexture(width, height, 4, LLViewerDynamicTexture::ORDER_LAST, false),
    // <FS:Ansariel> [Legacy Bake]
    mUploadPending(false), // Not used for any logic here, just to sync sending of updates
    mNeedsUpload(false),
    mNumLowresUploads(0),
    mUploadFailCount(0),
    // </FS:Ansariel> [Legacy Bake]
    mNeedsUpdate(true),
    mNumLowresUpdates(0)
{
    mGLTexturep->setNeedsAlphaAndPickMask(false);

    LLViewerTexLayerSetBuffer::sGLByteCount += getSize();
    // <FS:Ansariel> [Legacy Bake]
    mNeedsUploadTimer.start();
    mNeedsUpdateTimer.start();
}

LLViewerTexLayerSetBuffer::~LLViewerTexLayerSetBuffer()
{
    LLViewerTexLayerSetBuffer::sGLByteCount -= getSize();
    destroyGLTexture();
    for( S32 order = 0; order < ORDER_COUNT; order++ )
    {
        LLViewerDynamicTexture::sInstances[order].erase(this);  // will fail in all but one case.
    }
}

//virtual
S8 LLViewerTexLayerSetBuffer::getType() const
{
    return LLViewerDynamicTexture::LL_TEX_LAYER_SET_BUFFER ;
}

//virtual
void LLViewerTexLayerSetBuffer::restoreGLTexture()
{
    LLViewerDynamicTexture::restoreGLTexture() ;
}

//virtual
void LLViewerTexLayerSetBuffer::destroyGLTexture()
{
    LLViewerDynamicTexture::destroyGLTexture() ;
}

// static
void LLViewerTexLayerSetBuffer::dumpTotalByteCount()
{
    LL_INFOS() << "Composite System GL Buffers: " << (LLViewerTexLayerSetBuffer::sGLByteCount/1024) << "KB" << LL_ENDL;
}

void LLViewerTexLayerSetBuffer::requestUpdate()
{
    restartUpdateTimer();
    mNeedsUpdate = true;
    mNumLowresUpdates = 0;
    // <FS:Ansariel> [Legacy Bake]
    // If we're in the middle of uploading a baked texture, we don't care about it any more.
    // When it's downloaded, ignore it.
    mUploadID.setNull();
    // </FS:Ansariel> [Legacy Bake]
}

void LLViewerTexLayerSetBuffer::restartUpdateTimer()
{
    mNeedsUpdateTimer.reset();
    mNeedsUpdateTimer.start();
}

// virtual
bool LLViewerTexLayerSetBuffer::needsRender()
{
    llassert(mTexLayerSet->getAvatarAppearance() == gAgentAvatarp);
    if (!isAgentAvatarValid()) return false;

    // <FS:Ansariel> [Legacy Bake]
    const bool upload_now = mNeedsUpload && isReadyToUpload();
    const bool update_now = mNeedsUpdate && isReadyToUpdate();

    // Don't render if we don't want to (or aren't ready to) update.
    // <FS:Ansariel> [Legacy Bake]
    //if (!update_now)
    if (!(update_now || upload_now))
    {
        return false;
    }

    // Don't render if we're animating our appearance.
    if (gAgentAvatarp->getIsAppearanceAnimating())
    {
        return false;
    }

    // Don't render if we are trying to create a skirt texture but aren't wearing a skirt.
    if (gAgentAvatarp->getBakedTE(getViewerTexLayerSet()) == LLAvatarAppearanceDefines::TEX_SKIRT_BAKED &&
        !gAgentAvatarp->isWearingWearableType(LLWearableType::WT_SKIRT))
    {
        // <FS:Ansariel> [Legacy Bake]
        cancelUpload();
        return false;
    }

    // Render if we have at least minimal level of detail for each local texture.
    return getViewerTexLayerSet()->isLocalTextureDataAvailable();
}

// virtual
void LLViewerTexLayerSetBuffer::preRenderTexLayerSet()
{
    LLTexLayerSetBuffer::preRenderTexLayerSet();

    // keep depth buffer, we don't need to clear it
    LLViewerDynamicTexture::preRender(false);
}

// virtual
void LLViewerTexLayerSetBuffer::postRenderTexLayerSet(bool success)
{

    LLTexLayerSetBuffer::postRenderTexLayerSet(success);
    LLViewerDynamicTexture::postRender(success);
}

// virtual
// <FS:Ansariel> [Legacy Bake]
//void LLViewerTexLayerSetBuffer::midRenderTexLayerSet(bool success)
void LLViewerTexLayerSetBuffer::midRenderTexLayerSet(bool success, LLRenderTarget* bound_target)
// </FS:Ansariel> [Legacy Bake]
{
    // <FS:Ansariel> [Legacy Bake]
    // do we need to upload, and do we have sufficient data to create an uploadable composite?
    // TODO: When do we upload the texture if gAgent.mNumPendingQueries is non-zero?
    const bool upload_now = mNeedsUpload && isReadyToUpload();
    // </FS:Ansariel> [Legacy Bake]
    const bool update_now = mNeedsUpdate && isReadyToUpdate();

    // <FS:Ansariel> [Legacy Bake]
    if(upload_now)
    {
        if (!success)
        {
            LL_INFOS() << "Failed attempt to bake " << mTexLayerSet->getBodyRegionName() << LL_ENDL;
            mUploadPending = false;
        }
        else
        {
            LLViewerTexLayerSet* layer_set = getViewerTexLayerSet();
            if (layer_set->isVisible())
            {
                //<FS:Beq> OpenSim BOM fallback
                // layer_set->getAvatar()->debugBakedTextureUpload(layer_set->getBakedTexIndex(), false); // false for start of upload, true for finish.
                //doUpload(bound_target);
                auto bakedTexIdx = layer_set->getBakedTexIndex();
                if(bakedTexIdx <= layer_set->getAvatar()->getNumBakes())
                {
                    layer_set->getAvatar()->debugBakedTextureUpload(bakedTexIdx, false); // false for start of upload, true for finish.
                    doUpload(bound_target);
                }
                else
                {
                    LL_DEBUGS("Avatar") << "Skipping bake for unsupported layer on this region" << LL_ENDL;
                }
                // <FS:Beq>
            }
            else
            {
                mUploadPending = false;
                mNeedsUpload = false;
                mNeedsUploadTimer.pause();
                layer_set->getAvatar()->setNewBakedTexture(layer_set->getBakedTexIndex(),IMG_INVISIBLE);
            }
        }
    }
    // </FS:Ansariel> [Legacy Bake]

    if (update_now)
    {
        doUpdate();
    }

    // *TODO: Old logic does not check success before setGLTextureCreated
    // we have valid texture data now
    mGLTexturep->setGLTextureCreated(true);
}

bool LLViewerTexLayerSetBuffer::isInitialized(void) const
{
    return mGLTexturep.notNull() && mGLTexturep->isGLTextureCreated();
}

bool LLViewerTexLayerSetBuffer::isReadyToUpdate() const
{
    // If we requested an update and have the final LOD ready, then update.
    if (getViewerTexLayerSet()->isLocalTextureDataFinal()) return true;

    // If we haven't done an update yet, then just do one now regardless of state of textures.
    if (mNumLowresUpdates == 0) return true;

    // Update if we've hit a timeout.  Unlike for uploads, we can make this timeout fairly small
    // since render unnecessarily doesn't cost much.
    const U32 TEXTURE_TIMEOUT = 10;
    if (TEXTURE_TIMEOUT != 0)
    {
        // If we hit our timeout and have textures available at even lower resolution, then update.
        const bool is_update_textures_timeout = mNeedsUpdateTimer.getElapsedTimeF32() >= TEXTURE_TIMEOUT;
        const bool has_lower_lod = getViewerTexLayerSet()->isLocalTextureDataAvailable();
        if (has_lower_lod && is_update_textures_timeout) return true;
    }

    return false;
}

bool LLViewerTexLayerSetBuffer::requestUpdateImmediate()
{
    mNeedsUpdate = true;
    bool result = false;

    if (needsRender())
    {
        preRender(false);
        result = render();
        postRender(result);
    }

    return result;
}

// Mostly bookkeeping; don't need to actually "do" anything since
// render() will actually do the update.
void LLViewerTexLayerSetBuffer::doUpdate()
{
    LLViewerTexLayerSet* layer_set = getViewerTexLayerSet();
    const bool highest_lod = layer_set->isLocalTextureDataFinal();
    if (highest_lod)
    {
        mNeedsUpdate = false;
    }
    else
    {
        mNumLowresUpdates++;
    }

    restartUpdateTimer();

    // need to switch to using this layerset if this is the first update
    // after getting the lowest LOD
    layer_set->getAvatar()->updateMeshTextures();

    // Print out notification that we updated this texture.
    if (gSavedSettings.getBOOL("DebugAvatarRezTime"))
    {
        const bool highest_lod = layer_set->isLocalTextureDataFinal();
        const std::string lod_str = highest_lod ? "HighRes" : "LowRes";
        LLSD args;
        args["EXISTENCE"] = llformat("%d",(U32)layer_set->getAvatar()->debugGetExistenceTimeElapsedF32());
        args["TIME"] = llformat("%d",(U32)mNeedsUpdateTimer.getElapsedTimeF32());
        args["BODYREGION"] = layer_set->getBodyRegionName();
        args["RESOLUTION"] = lod_str;
        LLNotificationsUtil::add("AvatarRezSelfBakedTextureUpdateNotification",args);
        LL_DEBUGS("Avatar") << self_av_string() << "Locally updating [ name: " << layer_set->getBodyRegionName() << " res:" << lod_str << " time:" << (U32)mNeedsUpdateTimer.getElapsedTimeF32() << " ]" << LL_ENDL;
    }
}

//-----------------------------------------------------------------------------
// LLViewerTexLayerSet
// An ordered set of texture layers that get composited into a single texture.
//-----------------------------------------------------------------------------

LLViewerTexLayerSet::LLViewerTexLayerSet(LLAvatarAppearance* const appearance) :
    LLTexLayerSet(appearance),
    mUpdatesEnabled( false )
{
}

// virtual
LLViewerTexLayerSet::~LLViewerTexLayerSet()
{
}

// Returns true if at least one packet of data has been received for each of the textures that this layerset depends on.
bool LLViewerTexLayerSet::isLocalTextureDataAvailable() const
{
    if (!mAvatarAppearance->isSelf()) return false;
    return getAvatar()->isLocalTextureDataAvailable(this);
}


// Returns true if all of the data for the textures that this layerset depends on have arrived.
bool LLViewerTexLayerSet::isLocalTextureDataFinal() const
{
    if (!mAvatarAppearance->isSelf()) return false;
    return getAvatar()->isLocalTextureDataFinal(this);
}

// virtual
void LLViewerTexLayerSet::requestUpdate()
{
    if( mUpdatesEnabled )
    {
        createComposite();
        getViewerComposite()->requestUpdate();
    }
}

void LLViewerTexLayerSet::updateComposite()
{
    createComposite();
    getViewerComposite()->requestUpdateImmediate();
}

// virtual
void LLViewerTexLayerSet::createComposite()
{
    if(!mComposite)
    {
        S32 width = mInfo->getWidth();
        S32 height = mInfo->getHeight();
        // Composite other avatars at reduced resolution
        if( !mAvatarAppearance->isSelf() )
        {
            LL_ERRS() << "composites should not be created for non-self avatars!" << LL_ENDL;
        }
        mComposite = new LLViewerTexLayerSetBuffer( this, width, height );
    }
}

void LLViewerTexLayerSet::setUpdatesEnabled( bool b )
{
    mUpdatesEnabled = b;
}

LLVOAvatarSelf* LLViewerTexLayerSet::getAvatar()
{
    return dynamic_cast<LLVOAvatarSelf*> (mAvatarAppearance);
}

const LLVOAvatarSelf* LLViewerTexLayerSet::getAvatar() const
{
    return dynamic_cast<const LLVOAvatarSelf*> (mAvatarAppearance);
}

LLViewerTexLayerSetBuffer* LLViewerTexLayerSet::getViewerComposite()
{
    return dynamic_cast<LLViewerTexLayerSetBuffer*> (getComposite());
}

const LLViewerTexLayerSetBuffer* LLViewerTexLayerSet::getViewerComposite() const
{
    return dynamic_cast<const LLViewerTexLayerSetBuffer*> (getComposite());
}


const std::string LLViewerTexLayerSetBuffer::dumpTextureInfo() const
{
    if (!isAgentAvatarValid()) return "";

    // <FS:Ansariel> [Legacy Bake]
    //const bool is_high_res = true;
    //const U32 num_low_res = 0;
    //const std::string local_texture_info = gAgentAvatarp->debugDumpLocalTextureDataInfo(getViewerTexLayerSet());

    //std::string text = llformat("[HiRes:%d LoRes:%d] %s",
    //                          is_high_res, num_low_res,
    //                          local_texture_info.c_str());
    const bool is_high_res = !mNeedsUpload;
    const U32 num_low_res = mNumLowresUploads;
    const U32 upload_time = (U32)mNeedsUploadTimer.getElapsedTimeF32();
    const std::string local_texture_info = gAgentAvatarp->debugDumpLocalTextureDataInfo(getViewerTexLayerSet());

    std::string status              = "CREATING ";
    if (!uploadNeeded()) status     = "DONE     ";
    if (uploadInProgress()) status  = "UPLOADING";

    std::string text = llformat("[%s] [HiRes:%d LoRes:%d] [Elapsed:%d] %s",
                                status.c_str(),
                                is_high_res, num_low_res,
                                upload_time,
                                local_texture_info.c_str());
    // </FS:Ansariel> [Legacy Bake]
    return text;
}


// <FS:Ansariel> [Legacy Bake]
//-----------------------------------------------------------------------------
// Legacy baking
//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
// LLBakedUploadData()
//-----------------------------------------------------------------------------
LLBakedUploadData::LLBakedUploadData(const LLVOAvatarSelf* avatar,
                                     LLViewerTexLayerSet* layerset,
                                     const LLUUID& id,
                                     bool highest_res) :
    mAvatar(avatar),
    mTexLayerSet(layerset),
    mID(id),
    mStartTime(LLFrameTimer::getTotalTime()),       // Record starting time
    mIsHighestRes(highest_res)
{
}

void LLViewerTexLayerSetBuffer::requestUpload()
{
    conditionalRestartUploadTimer();
    mNeedsUpload = true;
    mNumLowresUploads = 0;
    mUploadPending = true;
}

void LLViewerTexLayerSetBuffer::conditionalRestartUploadTimer()
{
    // If we requested a new upload but haven't even uploaded
    // a low res version of our last upload request, then
    // keep the timer ticking instead of resetting it.
    if (mNeedsUpload && (mNumLowresUploads == 0))
    {
        mNeedsUploadTimer.unpause();
    }
    else
    {
        mNeedsUploadTimer.reset();
        mNeedsUploadTimer.start();
    }
}

void LLViewerTexLayerSetBuffer::cancelUpload()
{
    mNeedsUpload = false;
    mUploadPending = false;
    mNeedsUploadTimer.pause();
    mUploadRetryTimer.reset();
}

bool LLViewerTexLayerSetBuffer::uploadPending() const
{
    return mUploadPending;
}

bool LLViewerTexLayerSetBuffer::uploadNeeded() const
{
    return mNeedsUpload;
}

bool LLViewerTexLayerSetBuffer::uploadInProgress() const
{
    return !mUploadID.isNull();
}

bool LLViewerTexLayerSetBuffer::isReadyToUpload() const
{
    if (!gAgentQueryManager.hasNoPendingQueries()) return false; // Can't upload if there are pending queries.
    if (isAgentAvatarValid() && gAgentAvatarp->isEditingAppearance()) return false; // Don't upload if avatar is being edited.

    bool ready{ false };
    if (getViewerTexLayerSet()->isLocalTextureDataFinal())
    {
        // If we requested an upload and have the final LOD ready, upload (or wait a while if this is a retry)
        if (mUploadFailCount == 0)
        {
            ready = true;
        }
        else
        {
            ready = mUploadRetryTimer.getElapsedTimeF32() >= BAKE_UPLOAD_RETRY_DELAY * (1 << (mUploadFailCount - 1));
        }
    }
    else
    {
        // Upload if we've hit a timeout.  Upload is a pretty expensive process so we need to make sure
        // we aren't doing uploads too frequently.
        const U32 texture_timeout = gSavedSettings.getU32("AvatarBakedTextureUploadTimeout");
        if (texture_timeout != 0)
        {
            // The timeout period increases exponentially between every lowres upload in order to prevent
            // spamming the server with frequent uploads.
            const U32 texture_timeout_threshold = texture_timeout*(1 << mNumLowresUploads);

            // If we hit our timeout and have textures available at even lower resolution, then upload.
            const bool is_upload_textures_timeout = mNeedsUploadTimer.getElapsedTimeF32() >= texture_timeout_threshold;
            const bool has_lower_lod = getViewerTexLayerSet()->isLocalTextureDataAvailable();
            ready = has_lower_lod && is_upload_textures_timeout;
        }
    }

    return ready;
}

class FSTexlayerUpload: public LLBufferedAssetUploadInfo
{
    LLBakedUploadData *mBakdedUploadData;
public:
    FSTexlayerUpload( LLUUID aAssetId, LLBakedUploadData *aBakdedUploadData, std::string aTexture )
        : LLBufferedAssetUploadInfo( LLUUID::null, LLAssetType::AT_TEXTURE, aTexture, nullptr, nullptr)
        , mBakdedUploadData( aBakdedUploadData )
    {
        setAssetId( aAssetId );
    }

    ~FSTexlayerUpload()
    {
        delete mBakdedUploadData;
    }

    LLSD prepareUpload()
    {
        return LLSD().with("success", LLSD::Boolean(true));
    }

    LLSD generatePostBody()
    {
        return LLBufferedAssetUploadInfo::generatePostBody();;
    }

    LLUUID finishUpload(LLSD &aResult)
    {
        LLCore::HttpStatus status = LLCoreHttpUtil::HttpCoroutineAdapter::getStatusFromLLSD( aResult[ LLCoreHttpUtil::HttpCoroutineAdapter::HTTP_RESULTS ] );

        LLUUID newAssetId = aResult["new_asset"].asUUID();

        if( HTTP_OK == status.getType() )
        {
            std::string result = aResult["state"];

            LL_INFOS() << "result: " << ll_pretty_print_sd( aResult ) << " new_id: " << newAssetId << LL_ENDL;
            if (result == "complete"
                && mBakdedUploadData != nullptr)
            {// Invoke
                LLViewerTexLayerSetBuffer::onTextureUploadComplete(newAssetId, (void*) mBakdedUploadData, 0, LLExtStat::NONE);
                mBakdedUploadData = nullptr;// deleted in onTextureUploadComplete()
            }
            else
            {// Invoke the original callback with an error result
                LLViewerTexLayerSetBuffer::onTextureUploadComplete(newAssetId, (void*) mBakdedUploadData, -1, LLExtStat::NONE);
                mBakdedUploadData = nullptr;// deleted in onTextureUploadComplete()
            }
        }
        else
        {
            LL_WARNS() << "Uploading baked texture resulted in http " << status.getType() << ll_pretty_print_sd( aResult ) << LL_ENDL;
            // Invoke the original callback with an error result
            LLViewerTexLayerSetBuffer::onTextureUploadComplete(LLUUID(), (void*) mBakdedUploadData, -1, LLExtStat::NONE);
            mBakdedUploadData = nullptr;// deleted in onTextureUploadComplete()
        }

        return newAssetId;
    }
};

// Create the baked texture, send it out to the server, then wait for it to come
// back so we can switch to using it.
void LLViewerTexLayerSetBuffer::doUpload(LLRenderTarget* bound_target)
{
    LLViewerTexLayerSet* layer_set = getViewerTexLayerSet();
    LL_DEBUGS("Avatar") << "Uploading baked " << layer_set->getBodyRegionName() << LL_ENDL;
    add(LLStatViewer::TEX_BAKES, 1);

    // Don't need caches since we're baked now.  (note: we won't *really* be baked
    // until this image is sent to the server and the Avatar Appearance message is received.)
    layer_set->deleteCaches();

    // Get the COLOR information from our texture
    U8* baked_color_data = new U8[ mFullWidth * mFullHeight * 4 ];
    glReadPixels(mOrigin.mX, mOrigin.mY, mFullWidth, mFullHeight, GL_RGBA, GL_UNSIGNED_BYTE, baked_color_data );
    stop_glerror();

    // Get the MASK information from our texture
    LLGLSUIDefault gls_ui;
    LLPointer<LLImageRaw> baked_mask_image = new LLImageRaw(mFullWidth, mFullHeight, 1 );
    U8* baked_mask_data = baked_mask_image->getData();
    layer_set->gatherMorphMaskAlpha(baked_mask_data,
                                    mOrigin.mX, mOrigin.mY,
                                    mFullWidth, mFullHeight, bound_target);


    // Create the baked image from our color and mask information
    const S32 baked_image_components = 5; // red green blue [bump] clothing
    LLPointer<LLImageRaw> baked_image = new LLImageRaw( mFullWidth, mFullHeight, baked_image_components );
    U8* baked_image_data = baked_image->getData();
    S32 i = 0;
    for (U32 u=0; u < mFullWidth; u++)
    {
        for (U32 v=0; v < mFullHeight; v++)
        {
            baked_image_data[5*i + 0] = baked_color_data[4*i + 0];
            baked_image_data[5*i + 1] = baked_color_data[4*i + 1];
            baked_image_data[5*i + 2] = baked_color_data[4*i + 2];
            baked_image_data[5*i + 3] = baked_color_data[4*i + 3]; // alpha should be correct for eyelashes.
            baked_image_data[5*i + 4] = baked_mask_data[i];
            i++;
        }
    }

    LLPointer<LLImageJ2C> compressedImage = new LLImageJ2C;
    const char* comment_text = LINDEN_J2C_COMMENT_PREFIX "RGBHM"; // writes into baked_color_data. 5 channels (rgb, heightfield/alpha, mask)
    if (compressedImage->encode(baked_image, comment_text))
    {
        LLTransactionID tid;
        tid.generate();
        const LLAssetID asset_id = tid.makeAssetID(gAgent.getSecureSessionID());
        LLFileSystem up_file(asset_id, LLAssetType::AT_TEXTURE, LLFileSystem::WRITE);
        if (up_file.write(compressedImage->getData(), compressedImage->getDataSize()))
        {
            // Read back the file and validate.
            bool valid = false;
            LLPointer<LLImageJ2C> integrity_test = new LLImageJ2C;
            S32 file_size = 0;
            LLFileSystem file(asset_id, LLAssetType::AT_TEXTURE);
            file_size = file.getSize();
            U8* data = integrity_test->allocateData(file_size);
            std::string strAssetData;

            if (data)
            {
                file.read(data, file_size);
                strAssetData.append( reinterpret_cast< char const*> ( data ), file_size );
                valid = integrity_test->validate(data, file_size); // integrity_test will delete 'data'
            }
            else
            {
                integrity_test->setLastError("Unable to read entire file");
            }

            if (valid)
            {
                const bool highest_lod = layer_set->isLocalTextureDataFinal();
                // Baked_upload_data is owned by the responder and deleted after the request completes.
                LLBakedUploadData* baked_upload_data = new LLBakedUploadData(gAgentAvatarp,
                                                                             layer_set,
                                                                             asset_id,
                                                                             highest_lod);
                // upload ID is used to avoid overlaps, e.g. when the user rapidly makes two changes outside of Face Edit.
                mUploadID = asset_id;

                // Upload the image
                const std::string url = gAgent.getRegionCapability("UploadBakedTexture");
                if(!url.empty()
                    && !LLPipeline::sForceOldBakedUpload // toggle debug setting UploadBakedTexOld to change between the new caps method and old method
                    && (mUploadFailCount < (BAKE_UPLOAD_ATTEMPTS - 1))) // Try last ditch attempt via asset store if cap upload is failing.
                {
                    LLSD body = LLSD::emptyMap();
                    // The responder will call LLViewerTexLayerSetBuffer::onTextureUploadComplete()
                    LLResourceUploadInfo::ptr_t uploadInfo(new FSTexlayerUpload( mUploadID, baked_upload_data, strAssetData ) );
                    LLViewerAssetUpload::EnqueueInventoryUpload(url, uploadInfo);

                    LL_INFOS() << "Baked texture upload via capability of " << mUploadID << " to " << url << LL_ENDL;
                }
                else
                {
                    gAssetStorage->storeAssetData(tid,
                                                  LLAssetType::AT_TEXTURE,
                                                  LLViewerTexLayerSetBuffer::onTextureUploadComplete,
                                                  baked_upload_data,
                                                  true,     // temp_file
                                                  true,     // is_priority
                                                  true);    // store_local
                    LL_INFOS() << "Baked texture upload via Asset Store." <<  LL_ENDL;
                }

                if (highest_lod)
                {
                    // Sending the final LOD for the baked texture.  All done, pause
                    // the upload timer so we know how long it took.
                    mNeedsUpload = false;
                    mNeedsUploadTimer.pause();
                }
                else
                {
                    // Sending a lower level LOD for the baked texture.  Restart the upload timer.
                    mNumLowresUploads++;
                    mNeedsUploadTimer.unpause();
                    mNeedsUploadTimer.reset();
                }

                // Print out notification that we uploaded this texture.
                if (gSavedSettings.getBOOL("DebugAvatarRezTime"))
                {
                    const std::string lod_str = highest_lod ? "HighRes" : "LowRes";
                    LLSD args;
                    args["EXISTENCE"] = llformat("%d",(U32)layer_set->getAvatar()->debugGetExistenceTimeElapsedF32());
                    args["TIME"] = llformat("%d",(U32)mNeedsUploadTimer.getElapsedTimeF32());
                    args["BODYREGION"] = layer_set->getBodyRegionName();
                    args["RESOLUTION"] = lod_str;
                    LLNotificationsUtil::add("AvatarRezSelfBakedTextureUploadNotification",args);
                    LL_DEBUGS("Avatar") << self_av_string() << "Uploading [ name: " << layer_set->getBodyRegionName() << " res:" << lod_str << " time:" << (U32)mNeedsUploadTimer.getElapsedTimeF32() << " ]" << LL_ENDL;
                }
            }
            else
            {
                // The read back and validate operation failed.  Remove the uploaded file.
                mUploadPending = false;
                LLFileSystem::removeFile(asset_id, LLAssetType::AT_TEXTURE);
                LL_INFOS() << "Unable to create baked upload file (reason: corrupted)." << LL_ENDL;
            }
        }
    }
    else
    {
        // The VFS write file operation failed.
        mUploadPending = false;
        LL_INFOS() << "Unable to create baked upload file (reason: failed to write file)" << LL_ENDL;
    }

    delete [] baked_color_data;
}

// static
void LLViewerTexLayerSetBuffer::onTextureUploadComplete(const LLUUID& uuid,
                                                  void* userdata,
                                                  S32 result,
                                                  LLExtStat ext_status) // StoreAssetData callback (not fixed)
{
    LLBakedUploadData* baked_upload_data = (LLBakedUploadData*)userdata;

    if (isAgentAvatarValid() &&
        !gAgentAvatarp->isDead() &&
        (baked_upload_data->mAvatar == gAgentAvatarp) && // Sanity check: only the user's avatar should be uploading textures.
        (baked_upload_data->mTexLayerSet->hasComposite()))
    {
        LLViewerTexLayerSetBuffer* layerset_buffer = baked_upload_data->mTexLayerSet->getViewerComposite();
        S32 failures = layerset_buffer->mUploadFailCount;
        layerset_buffer->mUploadFailCount = 0;

        if (layerset_buffer->mUploadID.isNull())
        {
            // The upload got canceled, we should be in the
            // process of baking a new texture so request an
            // upload with the new data

            // BAP: does this really belong in this callback, as
            // opposed to where the cancellation takes place?
            // suspect this does nothing.
            layerset_buffer->requestUpload();
        }
        else if (baked_upload_data->mID == layerset_buffer->mUploadID)
        {
            // This is the upload we're currently waiting for.
            layerset_buffer->mUploadID.setNull();
            const std::string name(baked_upload_data->mTexLayerSet->getBodyRegionName());
            const std::string resolution = baked_upload_data->mIsHighestRes ? " full res " : " low res ";
            if (result >= 0)
            {
                layerset_buffer->mUploadPending = false; // Allows sending of AgentSetAppearance later
                LLAvatarAppearanceDefines::ETextureIndex baked_te = gAgentAvatarp->getBakedTE(layerset_buffer->getViewerTexLayerSet());
                // Update baked texture info with the new UUID
                U64 now = LLFrameTimer::getTotalTime();     // Record starting time
                LL_INFOS() << "Baked" << resolution << "texture upload for " << name << " took " << (S32)((now - baked_upload_data->mStartTime) / 1000) << " ms" << LL_ENDL;
                gAgentAvatarp->setNewBakedTexture(baked_te, uuid);
            }
            else
            {
                ++failures;
                S32 max_attempts = baked_upload_data->mIsHighestRes ? BAKE_UPLOAD_ATTEMPTS : 1; // only retry final bakes
                LL_WARNS() << "Baked" << resolution << "texture upload for " << name << " failed (attempt " << failures << "/" << max_attempts << ")" << LL_ENDL;
                if (failures < max_attempts)
                {
                    layerset_buffer->mUploadFailCount = failures;
                    layerset_buffer->mUploadRetryTimer.start();
                    layerset_buffer->requestUpload();
                }
            }
        }
        else
        {
            LL_INFOS() << "Received baked texture out of date, ignored." << LL_ENDL;
        }

        gAgentAvatarp->dirtyMesh();
    }
    else
    {
        // Baked texture failed to upload (in which case since we
        // didn't set the new baked texture, it means that they'll try
        // and rebake it at some point in the future (after login?)),
        // or this response to upload is out of date, in which case a
        // current response should be on the way or already processed.
        LL_WARNS() << "Baked upload failed" << LL_ENDL;
    }

    delete baked_upload_data;
}

void LLViewerTexLayerSet::requestUpload()
{
    createComposite();
    getViewerComposite()->requestUpload();
}

void LLViewerTexLayerSet::cancelUpload()
{
    if(mComposite)
    {
        getViewerComposite()->cancelUpload();
    }
}
// </FS:Ansariel> [Legacy Bake]



