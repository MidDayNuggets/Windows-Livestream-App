#include <wx/wx.h>
#include <wx/image.h>

class MyFrame : public wxFrame
{
public:
    MyFrame(const wxString& title);

private:
    void OnPaint(wxPaintEvent& event);
    wxBitmap m_bitmap;

    wxDECLARE_EVENT_TABLE();
};

wxBEGIN_EVENT_TABLE(MyFrame, wxFrame)
EVT_PAINT(MyFrame::OnPaint)
wxEND_EVENT_TABLE()

class MyApp : public wxApp
{
public:
    virtual bool OnInit();
};

wxIMPLEMENT_APP(MyApp);

bool MyApp::OnInit()
{
    // Initialize the image handlers
    wxInitAllImageHandlers();

    MyFrame* frame = new MyFrame("Display Image in wxWidgets");
    frame->Show(true);
    return true;
}

MyFrame::MyFrame(const wxString& title) : wxFrame(NULL, wxID_ANY, title, wxDefaultPosition, wxSize(800, 600))
{
    // Load the image and convert it to wxBitmap
    wxImage image("requested_images/screen.jpeg", wxBITMAP_TYPE_JPEG);
    image.Rescale(500,500);
    m_bitmap = wxBitmap(image);
    
}

void MyFrame::OnPaint(wxPaintEvent& WXUNUSED(event))
{
    // Create a wxPaintDC object to handle drawing on the panel
    wxPaintDC dc(this);

    // Draw the bitmap at position (10, 10)
    if (m_bitmap.IsOk()) {
        dc.DrawBitmap(m_bitmap, 10, 10, false);
    }
}
