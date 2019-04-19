//
//  Output.cpp
//  PhotonicDirector
//
//  Created by Jur de Vries on 06/11/2017.
//

#include "Output.h"
#include "cinder/app/App.h"
#include "DMXPro.h"

DmxOutput::DmxOutput()
:mOut{0}, mWidth(320), mHeight(320), mDmxPro(nullptr), mDmxProIsConnected(false)
{
    gl::Fbo::Format format;
    format.colorTexture();
    mFbo = gl::Fbo::create(mWidth, mHeight, format);
    auto lambert = gl::ShaderDef().lambert().color();
    gl::GlslProgRef shader = gl::getStockShader( lambert );
    mRect = gl::Batch::create(geom::Rect(Rectf(0.f, 0.f, 1.f, 1.f)), shader);
    generateVisualizeTextures();
}

void DmxOutput::setChannelValue(int channel, int value)
{
    value = math<int>::clamp(value, 0, 255);
    mOut[channel - 1] = value;
}

void DmxOutput::setChannelValue(int channel, float value)
{
    int intValue = 0;
    if (value < 0.f) {
        intValue = 0;
    }
    else if (value > 1.0f) {
        intValue = 255;
    }
    else {
        intValue = 255 * value;
    }
    setChannelValue(channel, intValue);
}

int DmxOutput::getChannelValue(int channel)
{
    return mOut[channel - 1];
}

void DmxOutput::reset()
{
    std::fill_n(mOut, 512, 0);
}

void DmxOutput::update()
{
    if (mDmxPro != nullptr && mDmxPro->isConnected()) {
        for (int i = 0; i < 512; i++) {
            mDmxPro->setValue(mOut[i], i);
        }
    }
}

std::vector<std::string> DmxOutput::getDevicesList() {
    std::vector<std::string> devices = DMXPro::getDevicesList();
    std::vector<std::string> dmxDevices;
    for (auto deviceName : devices) {
        if (deviceName.find("tty.usbserial") != std::string::npos) {
            dmxDevices.push_back(deviceName);
        }
    }
    return dmxDevices;
}

void DmxOutput::connect(std::string deviceName)
{
    if (!mDmxProIsConnected) {
        console() << "Starting connection" << std::endl;
        mDmxPro = DMXPro::create(deviceName);
        mDmxProIsConnected = true;
    }
}

void DmxOutput::disConnect()
{
    mDmxPro = nullptr;
    mDmxProIsConnected = false;
}

bool DmxOutput::isConnected()
{
    return mDmxProIsConnected;
}

std::string DmxOutput::getConnectedDevice()
{
    if (mDmxProIsConnected) {
        return mDmxPro->getDeviceName();
    }
    return "";
}

bool DmxOutput::registerChannel(int channel, std::string uid)
{
    auto existingRecord = mChannelRegistry.find(channel);
    if (existingRecord == mChannelRegistry.end()) {
        mChannelRegistry[channel] = uid;
        return true;
    }
    else {
        if (existingRecord->second == uid) {
            return true;
        }
    }
    return false;
}

void DmxOutput::clearRegistry() {
    mChannelRegistry.clear();
}

bool DmxOutput::checkRangeAvailable(int channel, int channelAmount, std::string uuid) {
    for (int i = channel; i < channel + channelAmount; i++) {
        if (mChannelRegistry.find(i) != mChannelRegistry.end()) {
            if (mChannelRegistry.find(i)->second != uuid) {
                return false;
            }
        }
    }
    return true;
}

void DmxOutput::releaseChannels(std::string uid)
{
    if (! mChannelRegistry.empty()) {
        for (auto it = mChannelRegistry.begin(); it != mChannelRegistry.end(); ) {
            if (it->second == uid) {
                it = mChannelRegistry.erase(it);
                continue;
            }
            if (it != mChannelRegistry.end()) {
                it++;
            }
        }
    }
}

void DmxOutput::generateVisualizeTextures() {
    for (int i = 0; i < 256; i++) {
        TextLayout layout;
        layout.clear(ColorA(0.f, 0.f, 0.f, 0.f));
        layout.setFont(Font::getDefault());
        layout.setColor(Color::white());
        layout.addLine(std::to_string(i));
        Surface8u rendered = layout.render(TRUE, false);
        gl::Texture2dRef texture = gl::Texture2d::create(rendered);
        mValueTextures[i] = texture;
    }
}

void DmxOutput::visualize()
{
    gl::draw(getVisualizeTexture(), vec2(10,10));
}

gl::TextureRef DmxOutput::getVisualizeTexture()
{
    gl::ScopedFramebuffer fb(mFbo);
    gl::ScopedBlendAlpha sAb;
    gl::ScopedViewport vp(ivec2(0), mFbo->getSize());
    gl::pushMatrices();
    gl::setMatricesWindow(mFbo->getSize());
    gl::clear(Color(0.3f, 0.3f, 0.3f));
    int gutter = 2;
    int width = (mWidth / 16) - gutter;
    int height = (mHeight / 16) - gutter;
    int textMargin = 3;
    for (int i = 0; i < 16; i++ ) {
        float yPos = i * (width + gutter) + gutter / 2;
        for (int j = 0; j < 16; j++) {
            gl::pushMatrices();
            float xPos = j * (width + gutter) + gutter / 2;
            gl::translate(xPos, yPos);
            // Draw a rectangle.
            int value = mOut[i * 16 + j];
            gl::ScopedColor color(0.f, value / 256.f, 0.f);

            gl::pushMatrices();
            gl::scale(width, height);
            mRect->draw();
            gl::popMatrices();

            gl::ScopedColor textColor(Color::white());
            gl::draw(mValueTextures[value]);

            gl::popMatrices();

        }
    }
    gl::popMatrices();
    return mFbo->getColorTexture();
}
