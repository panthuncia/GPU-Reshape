// 
// The MIT License (MIT)
// 
// Copyright (c) 2024 Advanced Micro Devices, Inc.,
// Fatalist Development AB (Avalanche Studio Group),
// and Miguel Petersen.
// 
// All Rights Reserved.
// 
// Permission is hereby granted, free of charge, to any person obtaining a copy 
// of this software and associated documentation files (the "Software"), to deal 
// in the Software without restriction, including without limitation the rights 
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies 
// of the Software, and to permit persons to whom the Software is furnished to do so, 
// subject to the following conditions:
// 
// The above copyright notice and this permission notice shall be included in all 
// copies or substantial portions of the Software.
// 
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, 
// INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR 
// PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE 
// FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, 
// ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
// 

using System.Collections.Generic;
using Message.CLR;

namespace Studio.ViewModels.Workspace.Services
{
    public class ExecutionStackTraceService : IExecutionStackTraceService, Bridge.CLR.IBridgeListener
    {
        /// <summary>
        /// Connection for this listener
        /// </summary>
        public IConnectionViewModel? ConnectionViewModel
        {
            get => _connectionViewModel;
            set
            {
                _connectionViewModel = value;

                if (_connectionViewModel != null)
                {
                    OnConnectionChanged();
                }
            }
        }

        /// <summary>
        /// Try to get a host execution stack trace for a rolling execution identifier
        /// </summary>
        public string? GetStackTrace(uint rollingExecutionUID)
        {
            lock (this)
            {
                _stackTraces.TryGetValue(rollingExecutionUID, out string? stackTrace);
                return stackTrace;
            }
        }

        /// <summary>
        /// Invoked on connection changes
        /// </summary>
        private void OnConnectionChanged()
        {
            ConnectionViewModel!.Bridge?.Register(ExecutionStackTraceMessage.ID, this);
        }

        /// <summary>
        /// Invoked on destruction
        /// </summary>
        public void Destruct()
        {
            ConnectionViewModel?.Bridge?.Deregister(ExecutionStackTraceMessage.ID, this);
        }

        /// <summary>
        /// Bridge handler
        /// </summary>
        public void Handle(ReadOnlyMessageStream streams, uint count)
        {
            if (!streams.GetSchema().IsDynamic(ExecutionStackTraceMessage.ID))
            {
                return;
            }

            lock (this)
            {
                foreach (ExecutionStackTraceMessage message in new DynamicMessageView<ExecutionStackTraceMessage>(streams))
                {
                    _stackTraces[message.rollingExecutionUID] = message.stackTrace.String;
                }
            }
        }

        private IConnectionViewModel? _connectionViewModel;
        private readonly Dictionary<uint, string> _stackTraces = new();
    }
}