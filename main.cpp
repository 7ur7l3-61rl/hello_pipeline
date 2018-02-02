#include <iostream>

#include <gstreamermm.h>
#include <glibmm/main.h>

using namespace std;
using namespace Gst;
using Glib::RefPtr;

class PipelineContainer
{
public:

  void Initialize()
  {
    cout << __FUNCTION__ << ": Create Pipeline" << endl;
    pipeline = Pipeline::create();

    cout << __FUNCTION__ << ": Create Elements" << endl;
    sink = ElementFactory::create_element("appsink", "sink");
    decodebin = ElementFactory::create_element("decodebin", "decoder");
    source = ElementFactory::create_element("appsrc", "source");

    cout << __FUNCTION__ << ": Add the Elements to the Pipeline" << endl;
    pipeline->add(source)->add(decodebin)->add(sink);

    cout << __FUNCTION__ << ": Link the source Element to the Decoder" << endl;
    source->link(decodebin);
  }

  void SetUpMessageHandler(RefPtr<Glib::MainLoop> & main_loop) {
      cout << __FUNCTION__ << ": Add lambda slot for error or eos on bus" << endl;
      pipeline->get_bus()->add_watch([main_loop] (const RefPtr<Bus>&, const RefPtr<Message>& message) {
          Glib::ustring message_name = Enums::get_name(message->get_message_type());
          cout << "Message Handler: Message received: " << message_name << endl;
          switch (message->get_message_type())
          {
          case MessageType::MESSAGE_EOS:
              cout << "Message Handler: Got " << message_name << ", stopping message loop" << endl;
              main_loop->quit();
              break;
          case MessageType::MESSAGE_ERROR:
              cout << "Message Handler: Got " << message_name << ", stopping message loop" << endl;
              main_loop->quit();
              break;
          default:
              break;
          }
          return true;
      });
  }

  void ListenForPads() {
      cout << __FUNCTION__ << ": Add lambda slot for new pad on decodebin" << endl;
      decodebin->signal_pad_added().connect([&] (const RefPtr<Pad>& pad) {
          cout << "PadListener: Pad added to : " << decodebin->get_name() << endl;
          cout << "PadListener: Pad added    : " << pad->get_name() << endl;

          PadLinkReturn ret = pad->link(sink->get_static_pad("sink"));

          if (ret != PadLinkReturn::PAD_LINK_OK)
              cout << "PadListener: Pads could not be linked : " << ret << endl;
          else
              cout << "PadListener: Pads linked " << endl;
      });

  }

  void StartPipeline() {
      cout << __FUNCTION__ << ": Set Pipeline State to Playing " << endl;
      pipeline->set_state(State::STATE_PLAYING);
  }

  void StopPipeline() {
      cout << __FUNCTION__ << ": Set Pipeline State to Null " << endl;
      pipeline->set_state(State::STATE_NULL);
  }

private:

  RefPtr<Element> source;
  RefPtr<Element> decodebin;
  RefPtr<Element> sink;
  RefPtr<Pipeline> pipeline;
};

int main(int argc, char *argv[])
{
    cout << __FUNCTION__ << ": Initialize GStreamer" << endl;
    Gst::init(argc, argv);

    cout << __FUNCTION__ << ": Create Mainloop" << endl;
    RefPtr<Glib::MainLoop> main_loop = Glib::MainLoop::create();

    cout << __FUNCTION__ << ": Create PipelineContainer" << endl;
    PipelineContainer pipeline_container;

    cout << __FUNCTION__ << ": Initialize PipelineContainer" << endl;
    pipeline_container.Initialize();

    cout << __FUNCTION__ << ": Setup Message Handling" << endl;
    pipeline_container.SetUpMessageHandler(main_loop);

    cout << __FUNCTION__ << ": Setup Listener for Pads on Decodebin" << endl;
    pipeline_container.ListenForPads();

    cout << __FUNCTION__ << ": Start Pipeline" << endl;
    pipeline_container.StartPipeline();

    cout << __FUNCTION__ << ": Start Message Loop" << endl;
    main_loop->run();

    cout << __FUNCTION__ << ": Stop Pipeline" << endl;
    pipeline_container.StopPipeline();
    return 0;
}
