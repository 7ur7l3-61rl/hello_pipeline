#include <iostream>

#include <gstreamermm.h>
#include <gstreamermm/appsrc.h>

using namespace std;
using namespace Gst;
using Glib::RefPtr;

class AppSrcPluginTest
{
public:
  RefPtr<Element> source;
  RefPtr<Element> sink;
  RefPtr<Pipeline> pipeline;

  void CreatePipelineWithElements()
  {
    cout << __FUNCTION__ << " Create Pipeline" << endl;
    pipeline = Gst::Pipeline::create();

    sink = ElementFactory::create_element("fakesink", "sink");
    source = ElementFactory::create_element("appsrc", "source");

    pipeline->add(source)->add(sink);
    source->link(sink);
  }
};

int main()
{
    cout << "Start test" << endl;
    AppSrcPluginTest test;
    test.CreatePipelineWithElements();
    cout << "End test" << endl;
    return 0;
}
