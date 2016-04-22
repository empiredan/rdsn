/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2015 Microsoft Corporation
 * 
 * -=- Robust Distributed System Nucleus (rDSN) -=- 
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

/*
 * Description:
 *     application model atop of zion in c++ (layer 2)
 *
 * Revision history:
 *     Mar., 2016, @imzhenyu (Zhenyu Guo), first version
 *     xxxx-xx-xx, author, fix bug about xxx
 */

# pragma once

# include <dsn/cpp/service_app.h>

namespace dsn 
{
    /*!
    @addtogroup app-model
    @{
    */

    class replicated_service_app_type_1 : public service_app
    {
    public:
        replicated_service_app_type_1()  { }

        virtual ~replicated_service_app_type_1(void) {}

        //
        // for stateful apps with layer 2 support
        //
        virtual ::dsn::error_code checkpoint() { return ERR_NOT_IMPLEMENTED; }

        virtual ::dsn::error_code checkpoint_async() { return ERR_NOT_IMPLEMENTED; }

        //
        // prepare an app-specific learning request (on learner, to be sent to learneee
        // and used by method get_checkpoint), so that the learning process is more efficient
        //
        // return value:
        //   0 - it is unnecessary to prepare a earn request
        //   <= capacity - learn request is prepared ready
        //   > capacity - buffer is not enough, caller should allocate a bigger buffer and try again
        //
        virtual int prepare_get_checkpoint(void* buffer, int capacity) { return 0; }

        // 
        // Learn [start, infinite) from remote replicas (learner)
        //
        // Must be thread safe.
        //
        // The learned checkpoint can be a complete checkpoint (0, infinite), or a delta checkpoint
        // [start, infinite), depending on the capability of the underlying implementation.
        // 
        // Note the files in learn_state are copied from dir /replica@remote/data to dir /replica@local/learn,
        // so when apply the learned file state, make sure using learn_dir() instead of data_dir() to get the
        // full path of the files.
        //
        // the given state is a plain buffer where applications should fill it carefully for all the learn-state
        // into this buffer, with the total written size in total-learn-state-size.
        // if the capcity is not enough, applications should return ERR_CAPACITY_EXCEEDED and put required
        // buffer size in total-learn-state-size field, and our replication frameworks will retry with this new size
        //
        virtual ::dsn::error_code get_checkpoint(
            int64_t start,
            void*   learn_request,
            int     learn_request_size,
            /* inout */ dsn_app_learn_state& state,
            int state_capacity
            ) = 0;

        //
        // [DSN_CHKPT_LEARN]
        // after learn the state from learner, apply the learned state to the local app
        //
        // Or,
        //
        // [DSN_CHKPT_COPY]
        // when an app only implement synchonous checkpoint, the primary replica
        // needs to copy checkpoint from secondaries instead of
        // doing checkpointing by itself, in order to not stall the normal
        // write operations.
        //
        // Postconditions:
        // * after apply_checkpoint() done, last_committed_decree() == last_durable_decree()
        // 
        virtual ::dsn::error_code apply_checkpoint(const dsn_app_learn_state& state, dsn_chkpt_apply_mode mode) = 0;
        
    public:
        static dsn_error_t app_checkpoint(void* app)
        {
            auto sapp = (replicated_service_app_type_1*)(app);
            return sapp->checkpoint();
        }

        static dsn_error_t app_checkpoint_async(void* app)
        {
            auto sapp = (replicated_service_app_type_1*)(app);
            return sapp->checkpoint_async();
        }

        static int app_prepare_get_checkpoint(void* app, void* buffer, int capacity)
        {
            auto sapp = (replicated_service_app_type_1*)(app);
            return sapp->prepare_get_checkpoint(buffer, capacity);
        }
        
        static dsn_error_t app_get_checkpoint(
            void*   app,
            int64_t start_decree,
            void*   learn_request,
            int     learn_request_size,
            /* inout */ dsn_app_learn_state* state,
            int state_capacity
            )
        {
            auto sapp = (replicated_service_app_type_1*)(app);
            return sapp->get_checkpoint(start_decree, learn_request, learn_request_size, *state, state_capacity);
        }
        
        static dsn_error_t app_apply_checkpoint(void* app, const dsn_app_learn_state* state, dsn_chkpt_apply_mode mode)
        {
            auto sapp = (replicated_service_app_type_1*)(app);
            return sapp->apply_checkpoint(*state, mode);
        }
    };

    /*! C++ wrapper of the \ref dsn_register_app function for layer 1 */
    template<typename TServiceApp>
    void register_app_with_type_1_replication_support(const char* type_name)
    {
        dsn_app app;
        memset(&app, 0, sizeof(app));
        app.mask = DSN_L2_REPLICATION_APP_TYPE_1;
        strncpy(app.type_name, type_name, sizeof(app.type_name));
        app.layer1.create = service_app::app_create<TServiceApp>;
        app.layer1.start = service_app::app_start;
        app.layer1.destroy = service_app::app_destroy;

        app.layer2_apps_type_1.chkpt = replicated_service_app_type_1::app_checkpoint;
        app.layer2_apps_type_1.chkpt_async = replicated_service_app_type_1::app_checkpoint_async;
        app.layer2_apps_type_1.checkpoint_get_prepare = replicated_service_app_type_1::app_prepare_get_checkpoint;
        app.layer2_apps_type_1.chkpt_get = replicated_service_app_type_1::app_get_checkpoint;
        app.layer2_apps_type_1.chkpt_apply = replicated_service_app_type_1::app_apply_checkpoint;

        dsn_register_app(&app);
    }

    /*@}*/
} // end namespace dsn::service
