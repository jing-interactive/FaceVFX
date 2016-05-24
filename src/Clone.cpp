#include "Clone.h"
#include "cinder/app/App.h"
#include "cinder/gl/draw.h"
#include "cinder/gl/scoped.h"
#include "cinder/gl/shader.h"

using namespace ci;

void Clone::setup(int width, int height)
{
    gl::Fbo::Format fboFormat;
    fboFormat.enableDepthBuffer(false);

    mBufferFbo = gl::Fbo::create(width, height, fboFormat);
    mSrcBlurFbo = gl::Fbo::create(width, height, fboFormat);
    mDstBlurFbo = gl::Fbo::create(width, height, fboFormat);

    try
    {
        mMaskBlurShader = gl::GlslProg::create(app::loadAsset("shader/basic.vert"), app::loadAsset("shader/maskBlur.frag"));
        mMaskBlurShader->uniform("tex", 1);
        mMaskBlurShader->uniform("mask", 2);

        mCloneShader = gl::GlslProg::create(app::loadAsset("shader/basic.vert"), app::loadAsset("shader/clone.frag"));
        mCloneShader->uniform("src", 1);
        mCloneShader->uniform("srcBlur", 2);
        mCloneShader->uniform("dstBlur", 3);
    }
    catch (const std::exception& e)
    {
        app::console() << e.what() << std::endl;
    }

    mStrength = 0;
}

void Clone::maskedBlur(gl::TextureRef& tex, gl::TextureRef& mask, gl::FboRef& result)
{
    gl::ScopedTextureBind t2(mask, 2);
#if 1
    gl::ScopedGlslProg glsl(mMaskBlurShader);
#else
    gl::ScopedGlslProg glslTexOnly(gl::getStockShader(gl::ShaderDef().color()));
#endif

    {
        gl::ScopedFramebuffer fbo(mBufferFbo);
        gl::clear(ColorA::black(), false);
        gl::ScopedTextureBind t1(tex, 1);
        mMaskBlurShader->uniform("direction", vec2(1, 0));
        gl::drawSolidRect(tex->getBounds());
    }

    {
        gl::ScopedFramebuffer fbo(result);
        gl::clear(ColorA::black(), false);
        gl::ScopedTextureBind t1(mBufferFbo->getColorTexture(), 1);
        mMaskBlurShader->uniform("direction", vec2(0, 1));
        gl::drawSolidRect(tex->getBounds());
    }
}

void Clone::setStrength(int strength)
{
    mStrength = strength;
}

void Clone::update(gl::TextureRef& src, gl::TextureRef& dst, gl::TextureRef& mask)
{
    mMaskBlurShader->uniform("strength", mStrength);

    maskedBlur(src, mask, mSrcBlurFbo);
    maskedBlur(dst, mask, mDstBlurFbo);

    {
        gl::ScopedFramebuffer fbo(mBufferFbo);
        gl::ScopedBlendAlpha blend;
        gl::ScopedGlslProg glslTexOnly(gl::getStockShader(gl::ShaderDef().texture()));
        gl::draw(dst);

        gl::ScopedGlslProg glsl(mCloneShader);
        gl::ScopedTextureBind t1(src, 1);
        gl::ScopedTextureBind t2(mSrcBlurFbo->getColorTexture(), 2);
        gl::ScopedTextureBind t3(mDstBlurFbo->getColorTexture(), 3);
        gl::drawSolidRect(src->getBounds());
    }
}

void Clone::draw(vec2 pos)
{
    gl::draw(mBufferFbo->getColorTexture(), pos);
}
