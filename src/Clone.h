#pragma once

#include "cinder/gl/Texture.h"
#include "cinder/gl/GlslProg.h"
#include "cinder/gl/Fbo.h"

class Clone {
public:
	void setup(int width, int height);
	void setStrength(int strength);
    void update(ci::gl::TextureRef src, ci::gl::TextureRef dst, ci::gl::TextureRef mask);
    ci::gl::Texture2dRef getResultTexture();
    
protected:
    void maskedBlur(ci::gl::TextureRef tex, ci::gl::TextureRef mask, ci::gl::FboRef result);
    ci::gl::FboRef mBufferFbo, mSrcBlurFbo, mDstBlurFbo;
	ci::gl::GlslProgRef mMaskBlurShader, mCloneShader;
	int mStrength;
};