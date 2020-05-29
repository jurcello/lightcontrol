#include "cinder/app/App.h"
#include "cinder/app/RendererGl.h"
#include "cinder/gl/gl.h"
#include "cinder/params/Params.h"
#include "cinder/Log.h"
#include "CinderImGui.h"
#include "cinder/Json.h"
#include "Osc.h"
#include <boost/algorithm/string.hpp>
#include "Poco/Delegate.h"
#include "Poco/DNSSD/DNSSDResponder.h"
#include "Poco/DNSSD/DNSSDBrowser.h"
#include "Poco/DNSSD/Bonjour/Bonjour.h"
#include "Output.h"

using namespace ci;
using namespace ci::app;
using namespace std;

using protocol = asio::ip::udp;

const int WINDOW_WIDTH = 1024;
const int WINDOW_HEIGHT = 768;


bool validateIpAddress(const string &ipAddress)
{
    struct sockaddr_in sa;
    int result = inet_pton(AF_INET, ipAddress.c_str(), &(sa.sin_addr));
    return result != 0;
}

class LightControlApp : public App
{
  public:
    LightControlApp();

    virtual ~LightControlApp();

    void setTheme(ImGui::Options &options);

    void setup() override;
    void update() override;
    void draw() override;
    void keyDown(KeyEvent event) override;
    void onServiceFound(const void* sender, const Poco::DNSSD::DNSSDBrowser::ServiceEventArgs& args);
    void onServiceResolved(const void* sender, const Poco::DNSSD::DNSSDBrowser::ServiceEventArgs& args);
    void onError(const void* sender, const Poco::DNSSD::DNSSDResponder::ErrorEventArgs& args);
    void sendVolume();
    void sendCurrentValues();
    int getDmxChannel(int page, int column, int row);

    void setupOsc(int receivePort, int sendPort);

  protected:
    // Osc related stuff.
    osc::ReceiverUdp *mOscReceiver;
    osc::SenderUdp *mOscSender;
    osc::UdpSocketRef mOscSocket;
    bool mOscUnicast;
    std::string mOscSendAddress;
    int mOscReceivePort;
    int mOscSendPort;

    void oscReceive(const osc::Message &message);
    void drawGui();
    void drawDmxInspector();
    // Dmx output.
    DmxOutput mDmxOut;
    bool mDmxFound;
    int mChannelOutArray[512];
    float mVolume;
    void autoDiscoverDmx();

    // Zeroconf
    Poco::DNSSD::DNSSDResponder *mDnssdResponder;
    Poco::DNSSD::ServiceHandle mServiceHandle;
    Poco::DNSSD::BrowseHandle mBrowserHandle;
};

LightControlApp::LightControlApp()
    : mOscReceiver(nullptr),
      mOscSender(nullptr),
      mOscSocket(nullptr),
      mOscReceivePort(10000),
      mOscSendPort(10001),
      mOscUnicast(true),
      mOscSendAddress("192.168.1.11"),
      mVolume(0.f),
      mDmxFound(false),
      mChannelOutArray{0},
      mDnssdResponder(nullptr)
{
    Poco::DNSSD::initializeDNSSD();
}

void LightControlApp::setTheme(ImGui::Options &options)
{
    options = options.darkTheme();
    options.color(ImGuiCol_PlotHistogram, ImVec4(0.0f, 0.93f, 0.0f, 0.63f));
}

void LightControlApp::setup()
{
    mDnssdResponder = new Poco::DNSSD::DNSSDResponder();
    mDnssdResponder->start();
    mDnssdResponder->serviceRegistrationFailed += Poco::delegate(this, &LightControlApp::onError);
    mDnssdResponder->browser().serviceFound += Poco::delegate(this, &LightControlApp::onServiceFound);
    mDnssdResponder->browser().serviceResolved += Poco::delegate(this, &LightControlApp::onServiceResolved);
    mDnssdResponder->browser().serviceResolved += Poco::delegate(this, &LightControlApp::onServiceResolved);
    mBrowserHandle = mDnssdResponder->browser().browse("_osc._udp", "");

    ImGui::Options options;
    setTheme(options);
    ImGui::initialize(options);
    ImGui::connectWindow(getWindow());

    // Initialize params.
    setupOsc(mOscReceivePort, mOscSendPort);
    std::fill_n(mChannelOutArray, 512, 0);
}

void LightControlApp::setupOsc(int receivePort, int sendPort)
{
    // register with DNSSDResponder
    mDnssdResponder->unregisterService(mServiceHandle);
    std::string name = "Light Control  - " + std::to_string(receivePort);
    int dnssdReceivePort = receivePort;
    Poco::DNSSD::Service service(0, name, "", "_osc._udp", "", "", dnssdReceivePort);
    mServiceHandle = mDnssdResponder->registerService(service);

    if (mOscReceiver)
    {
        mOscReceiver->close();
        delete mOscReceiver;
    }
    if (mOscSender)
    {
        try
        {
            mOscSender->close();
            delete mOscSender;
        }
        catch (Exception exception)
        {
            console() << "An exception occured closing the connection";
        }
    }
    if (mOscSocket)
    {
        mOscSocket->close();
        mOscSocket = nullptr;
    }
    mOscReceiver = new osc::ReceiverUdp(receivePort);
    // Setup osc to listen to all addresses.
    mOscReceiver->setListener("/*", [&](const osc::Message &message) {
        oscReceive(message);
    });
    try
    {
        mOscReceiver->bind();
    }
    catch (const osc::Exception &ex)
    {
        CI_LOG_E("Error binding: " << ex.what() << ", val:" << ex.value());
    }
    mOscReceiver->listen([&](asio::error_code error, protocol::endpoint endpoint) -> bool {
        if (error)
        {
            if (error.value() != 89)
                CI_LOG_E("Error listening: " << error.message() << ", val: " << error.value() << ", endpoint: " << endpoint);
            return false;
        }
        else
        {
            return true;
        }
    });

    //////////////////////////
    // Setup sender.
    //////////////////////////
    // Us a local port of 31,000 because that one most probably is not used.
    const int localPort = 31000;
    mOscSocket = osc::UdpSocketRef(new protocol::socket(App::get()->io_service(), protocol::endpoint(protocol::v4(), localPort)));
    asio::ip::address_v4 address = asio::ip::address_v4::broadcast();
    try {
        if (mOscUnicast)
        {
            address = asio::ip::address_v4::from_string(mOscSendAddress);
        }
        else
        {
            mOscSocket->set_option(asio::socket_base::broadcast(true));
        }
        mOscSender = new osc::SenderUdp(mOscSocket, protocol::endpoint(address, sendPort));
        sendCurrentValues();
    }
    catch (...)
    {
        CI_LOG_E("Error setting up osc ");
    }
}

void LightControlApp::oscReceive(const osc::Message &message)
{
    if (mOscReceiver) {
        try {
            if (validateIpAddress(message.getSenderIpAddress().to_string())) {
                mOscSendAddress = message.getSenderIpAddress().to_string();
            }
            if (message.getAddress() == "/volume") {
                mVolume = message.getArgFloat(0);
            }
            if (message.getAddress() == "/volume") {
                mVolume = message.getArgFloat(0);
                sendVolume();
            } else {
                std::vector<char *> values;
                char *str = const_cast<char *>(message.getAddress().c_str());
                char *pch;
                pch = std::strtok(str, "/");
                while (pch != NULL) {
                    values.push_back(pch);
                    pch = std::strtok(NULL, "/");
                }

                // Start of parsing the addres to a dmx channel.
                std::string secondArg = "faders";
                if (values.size() == 4 && strcmp(values.at(1), secondArg.c_str()) == 0) {
                    int first = std::atoi(values.at(0));
                    int second = std::atoi(values.at(2));
                    int third = std::atoi(values.at(3));
                    int channel = (first - 1) * 42 + (third - 1) * 6 + second;
                    mChannelOutArray[channel - 1] = (int) message.getArgFloat(0);
                }
            }
        }
        catch (std::exception &exc) {
            app::console() << "Channel receives string or other unknown type: " << exc.what() << std::endl;
        }
    }
}

void LightControlApp::onServiceFound(const void* sender, const Poco::DNSSD::DNSSDBrowser::ServiceEventArgs& args)
{
    if (args.service.name().find("(TouchOSC") != std::string::npos) {
        reinterpret_cast<Poco::DNSSD::DNSSDBrowser*>(const_cast<void*>(sender))->resolve(args.service);
    }
}

void LightControlApp::onServiceResolved(const void* sender, const Poco::DNSSD::DNSSDBrowser::ServiceEventArgs& args)
{
//    this->mOscSendAddress = (std::string) args.service.host();
    this->mOscSendPort = args.service.port();
}

void LightControlApp::onError(const void *sender, const Poco::DNSSD::DNSSDResponder::ErrorEventArgs &args) {
    app::console()
            << "Service registration failed: "
            << args.error.message()
            << " (" << args.error.code() << ")"
            << std::endl;
}

void LightControlApp::keyDown(KeyEvent event)
{
    if (event.getChar() == 'q' && event.isAccelDown())
        exit(0);
}

void LightControlApp::draw()
{
    gl::clear(Color(0, 0, 0));
}

void LightControlApp::update()
{
    drawGui();

    // Prepare DMX output.
    mDmxOut.reset();
    for (int i = 0; i < 512; i++) {
        mDmxOut.setChannelValue(i+1, mChannelOutArray[i] * mVolume);
    }

    mDmxOut.update();
}

void LightControlApp::sendVolume() {
    if (mOscSender) {
        osc::Message message;
        message.setAddress("/volume");
        message.append(mVolume);
        mOscSender->send(message);
    }
}

void LightControlApp::sendCurrentValues() {
    if (mOscSender) {
        // 1/faders/column/row
        for (int page = 1; page < 3; page++) {
            osc::Bundle bundle;
            for (int column = 1; column < 7; column++) {
                for (int row = 1; row < 8; row++) {
                    string address =
                            "/" + std::to_string(page) + "/" + std::to_string(column) + "/" + std::to_string(row);
                    int channel = getDmxChannel(page, column, row);
                    osc::Message message;
                    message.setAddress(address);
                    message.append(mDmxOut.getChannelValue(channel));
                    bundle.append(message);
                }
            }
            mOscSender->send(bundle);
        }
    }
}

int LightControlApp::getDmxChannel(int page, int column, int row) {
    return (page - 1) * 42 + 6 * (row - 1) + column;
}

void LightControlApp::drawGui()
{
    static bool showDmxInspector = true;
    // Draw the general ui.
    ImGuiWindowFlags windowFlags = 0;
    windowFlags |= ImGuiWindowFlags_NoMove;
    ImGui::ScopedWindow window("Controls", windowFlags);

    ui::Separator();
    ui::Text("Osc settings");
    ui::Spacing();
    if (ui::InputInt("Osc Receive Port", &mOscReceivePort))
    {
        setupOsc(mOscReceivePort, mOscSendPort);
    }
    if (ui::InputInt("Osc Send Port", &mOscSendPort))
    {
        setupOsc(mOscReceivePort, mOscSendPort);
    }
    if (ui::Checkbox("Use udp unicast", &mOscUnicast))
    {
        setupOsc(mOscReceivePort, mOscSendPort);
    }
    if (mOscUnicast)
    {
        if (ui::InputText("Osc send address", &mOscSendAddress))
        {
            try
            {
                asio::ip::address_v4::from_string(mOscSendAddress);
                setupOsc(mOscReceivePort, mOscSendPort);
            }
            catch (std::exception e)
            {
                // Catch nothing.
            }
        }
    }

    ui::Separator();
    ui::Text("Dmx settings");
    if (!ui::IsWindowCollapsed())
    {
        if (!mDmxOut.isConnected())
        {
            auto devices = mDmxOut.getDevicesList();
            ui::ListBoxHeader("Choose device", devices.size());
            for (auto device : devices)
            {
                if (ui::Selectable(device.c_str()))
                {
                    mDmxOut.connect(device);
                }
            }
            ui::ListBoxFooter();
        }
        else
        {
            ui::Text("Connected to: ");
            const std::string deviceInfo = mDmxOut.getConnectedDevice();
            ui::Text("%s", deviceInfo.c_str());
            ui::SameLine();
            if (ui::Button("Disconnect"))
            {
                mDmxOut.disConnect();
            }
        }
    }
    ui::Separator();
    ui::Checkbox("Show DMX inspector", &showDmxInspector);
    ui::Separator();
    if (showDmxInspector)
    {
        drawDmxInspector();
    }
}

void LightControlApp::drawDmxInspector()
{
    ImGui::ScopedWindow window("Dmx inspector");
    if (!ui::IsWindowCollapsed())
    {
        auto dmxVisuals = mDmxOut.getVisualizeTexture();
        ui::Image(dmxVisuals, dmxVisuals->getSize());
    }
}

void LightControlApp::autoDiscoverDmx() {
    if (mDmxFound || mDmxOut.isConnected()) {
        return;
    }
    if (mDmxOut.getDevicesList().size() == 1 && ! mDmxOut.isConnected()) {
        auto devices = mDmxOut.getDevicesList();
        mDmxOut.connect(devices[0]);
        mDmxFound = true;
    }
}

LightControlApp::~LightControlApp()
{
    mDmxOut.disConnect();
    mDnssdResponder->unregisterService(mServiceHandle);
    mDnssdResponder->browser().cancel(mBrowserHandle);
    mDnssdResponder->stop();
    delete mDnssdResponder;
    Poco::DNSSD::uninitializeDNSSD();
}

CINDER_APP(LightControlApp, RendererGl(RendererGl::Options().msaa(1)), [](cinder::app::AppBase::Settings *settings) {
    settings->setTitle("Light Control");
    settings->setWindowSize(WINDOW_WIDTH, WINDOW_HEIGHT);
});
