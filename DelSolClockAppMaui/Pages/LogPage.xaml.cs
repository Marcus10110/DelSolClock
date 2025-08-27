using Microsoft.Maui.Controls;
using DelSolClockAppMaui.Services;

namespace DelSolClockAppMaui.Pages;

public partial class LogPage : ContentPage
{
    private DelSolConnection _ble = new();

    public LogPage()
    {
        InitializeComponent();
        foreach( var log in _ble.GetLogs() )
        {
            LogStack.Children.Add( new Label { Text = log } );
        }
    }
}