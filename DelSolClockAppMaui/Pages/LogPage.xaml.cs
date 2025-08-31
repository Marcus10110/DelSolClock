using Microsoft.Maui.Controls;
using DelSolClockAppMaui.Services;

namespace DelSolClockAppMaui.Pages;

public partial class LogPage : ContentPage
{

    public LogPage()
    {
        InitializeComponent();

        LogStack.Children.Add( new Label { Text = "Example Log" } );
    }
}