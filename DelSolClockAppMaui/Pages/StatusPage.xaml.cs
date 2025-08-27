using Microsoft.Maui.Controls;
using DelSolClockAppMaui.Services;

namespace DelSolClockAppMaui.Pages;

public partial class StatusPage : ContentPage
{
    private DelSolConnection _delSolConnection = new();

    public StatusPage()
    {
        InitializeComponent();
        StatusLabel.Text = _delSolConnection.Connected ? "Connected" : "Not Connected";
        DataList.ItemsSource = _delSolConnection.GetData().Select( kvp => $"{kvp.Key}: {kvp.Value}" );
    }
}