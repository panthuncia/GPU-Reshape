using System.Text;
using Message.CLR;
using Studio.ViewModels.Workspace;
using Studio.ViewModels.Workspace.Properties;
using Studio.ViewModels.Workspace.Services;

namespace Runtime.Utils.Workspace;

public static class TracebackUtils
{
    /// <summary>
    /// Format a traceback model
    /// </summary>
    public static string Format(IWorkspaceViewModel workspace, Traceback traceback)
    {
        StringBuilder builder = new();
        builder.Append(Format(traceback.executionFlag));

        // TODO: Pipeline collection
        builder.Append($", Pipeline {traceback.pipelineUid}");

        // Thread indices
        builder.Append($", Thread [{traceback.threadX}, {traceback.threadY}, {traceback.threadZ}]");
        
        // Launch dimensions
        if (traceback.executionFlag.HasFlag(ExecutionFlag.Dispatch))
        {
            builder.Append($", Thread Groups [{traceback.kernelLaunchX}, {traceback.kernelLaunchY}, {traceback.kernelLaunchZ}]");
        }

        string? stackTrace = workspace.PropertyCollection.GetService<IExecutionStackTraceService>()?.GetStackTrace(traceback.rollingExecutionUID);
        if (!string.IsNullOrWhiteSpace(stackTrace))
        {
            builder.Append($"\nHost Stack Trace:\n{stackTrace}");
        }
        else if (traceback.rollingExecutionUID == 0)
        {
            builder.Append("\nHost Stack Trace: unavailable (missing execution correlation ID)");
        }
        else
        {
            builder.Append($"\nHost Stack Trace: unavailable (no host trace captured for execution UID {traceback.rollingExecutionUID})");
        }

        return builder.ToString();
    }

    /// <summary>
    /// Format a traceback execution flag
    /// </summary>
    public static string Format(ExecutionFlag executionFlag)
    {
        StringBuilder builder = new();

        if (executionFlag.HasFlag(ExecutionFlag.Indirect))
        {
            builder.Append("Indirect ");
        }

        if (executionFlag.HasFlag(ExecutionFlag.Draw))
        {
            builder.Append("Draw");
        }
        else if (executionFlag.HasFlag(ExecutionFlag.Dispatch))
        {
            builder.Append("Dispatch");
        }
        else if (executionFlag.HasFlag(ExecutionFlag.Raytracing))
        {
            builder.Append("Raytracing");
        }

        if (builder.Length == 0)
        {
            builder.Append("Unknown");
        }
        
        return builder.ToString();
    }
}
