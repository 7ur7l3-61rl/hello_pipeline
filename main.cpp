#include <iostream>

#include <gstreamermm.h>
#include <gstreamermm/appsink.h>
#include <gstreamermm/appsrc.h>
#include <gstreamermm/decodebin.h>

using namespace std;
using namespace Gst;
using Glib::RefPtr;

class AppSrcPluginTest
{
public:

  void CreatePipelineWithElements()
  {
    cout << __FUNCTION__ << " Create Pipeline" << endl;
    pipeline = Gst::Pipeline::create();

    cout << __FUNCTION__ << " Create Elements" << endl;
    sink = ElementFactory::create_element("appsink", "sink");
    decodebin = Gst::ElementFactory::create_element("decodebin", "decoder");
    source = ElementFactory::create_element("appsrc", "source");

    cout << __FUNCTION__ << " Add the Elements to the Pipeline" << endl;
    pipeline->add(source)->add(decodebin)->add(sink);

    cout << __FUNCTION__ << " Link the source Element to the Decoder" << endl;
    source->link(decodebin);
  }

private:

  RefPtr<Element> source;
  RefPtr<Element> decodebin;
  RefPtr<Element> sink;
  RefPtr<Pipeline> pipeline;
};

int main()
{
    cout << "Start test" << endl;
    AppSrcPluginTest test;
    test.CreatePipelineWithElements();
    cout << "End test" << endl;
    return 0;
}
