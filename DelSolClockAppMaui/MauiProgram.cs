using Microsoft.Extensions.Logging;
using DelSolClockAppMaui.Services;
using DelSolClockAppMaui.Pages;

namespace DelSolClockAppMaui
{
    public static class MauiProgram
    {
        public static MauiApp CreateMauiApp()
        {
            var builder = MauiApp.CreateBuilder();
            builder
                .UseMauiApp<App>()
                .ConfigureFonts(fonts =>
                {
                    fonts.AddFont("OpenSans-Regular.ttf", "OpenSansRegular");
                    fonts.AddFont("OpenSans-Semibold.ttf", "OpenSansSemibold");
                });

            // Register BLE services
            builder.Services.AddSingleton<DelSolDevice>();
            
            // Register pages
            builder.Services.AddTransient<StatusPage>();
            builder.Services.AddTransient<UpdatePage>();
            builder.Services.AddTransient<DebugPage>();
            builder.Services.AddTransient<LogPage>();

            // Configure logging
#if DEBUG
            builder.Logging.AddDebug();
            builder.Logging.SetMinimumLevel(LogLevel.Debug);
#else
            builder.Logging.SetMinimumLevel(LogLevel.Information);
#endif

            return builder.Build();
        }
    }
}
