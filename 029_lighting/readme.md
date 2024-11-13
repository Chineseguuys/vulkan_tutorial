# 纹理贴图渲染错误

```cpp
void recordCommandBuffer(VkCommandBuffer commandBuffer, uint32_t imageIndex) {
        VkCommandBufferBeginInfo beginInfo{};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

        if (vkBeginCommandBuffer(commandBuffer, &beginInfo) != VK_SUCCESS) {
            spdlog::error("{} failed to begin recording command buffer");
            throw std::runtime_error("failed to begin recording command buffer");
        }

        VkRenderPassBeginInfo renderPassInfo{};
        renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        renderPassInfo.renderPass = mRenderPass;
        renderPassInfo.framebuffer = mSwapChainFramebuffers[imageIndex];
        renderPassInfo.renderArea.offset = {0, 0};
        renderPassInfo.renderArea.extent = mSwapChainExtent;

        std::array<VkClearValue, 2> clearValues{};
#ifndef USE_SELF_DEFINED_CLEAR_COLOR
        clearValues[0].color = {{0.0f, 0.0f, 0.0f, 1.0f}};
#else
        clearValues[0].color = {{0.102f, 0.102f, 0.102f, 1.0f}};
#endif /* USE_SELF_DEFINED_CLEAR_COLOR */
        clearValues[1].depthStencil = {1.0f, 0};

        renderPassInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
        renderPassInfo.pClearValues = clearValues.data();

        vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

        vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, mGraphicsPipeline);

        VkViewport viewport{};
        viewport.x = 0.0f;
        viewport.y = 0.0f;
        viewport.width = static_cast<float>(mSwapChainExtent.width);
        viewport.height = static_cast<float>(mSwapChainExtent.height);
        viewport.minDepth = 0.0f;
        viewport.maxDepth = 1.0f;
        vkCmdSetViewport(commandBuffer, 0, 1, &viewport);

        VkRect2D scissor{};
        scissor.offset = {0, 0};
        scissor.extent = {mSwapChainExtent.width, mSwapChainExtent.height};
        vkCmdSetScissor(commandBuffer, 0, 1, &scissor);

        for (auto& mesh : mMeshes) {
            auto& meshIndices = mesh.mIndices;
            // 需要更新 uboIndex
            spdlog::trace("{} update texture index: {}, {}", __func__, imageIndex, mesh.mMeterial_ID);
            updateTextureIndex(mCurrentFrame, mesh.mMeterial_ID);
            // vertex buffer
            VkBuffer vertexBuffers[] = {mVertexBuffer};
            VkDeviceSize offsets[] = {0};
            VkDeviceSize indicesOffsets = mesh.mOffset * sizeof(uint32_t);
            vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers, offsets);
            vkCmdBindIndexBuffer(commandBuffer, mIndexBuffer, indicesOffsets, VK_INDEX_TYPE_UINT32);
            vkCmdBindDescriptorSets(commandBuffer,
                VK_PIPELINE_BIND_POINT_GRAPHICS,
                mPipelineLayout,
                0,
                1,
                &mDescriptorSets[mCurrentFrame],
                0,
                nullptr
            );

            // ready to issue the draw command for the triangle
            // three vertices and draw one triangle
            //vkCmdDraw(commandBuffer, static_cast<uint32_t>(vertices.size()), 1, 0, 0);
            // Todo: 对于每一个 Shape (单独的模型) 我们都会使用不同的纹理，使用不同的 indices.
            vkCmdDrawIndexed(commandBuffer, static_cast<uint32_t>(meshIndices.size()), 1, 0, 0, 0);
        }

        vkCmdEndRenderPass(commandBuffer);

        if (vkEndCommandBuffer(commandBuffer) != VK_SUCCESS) {
            spdlog::error("{} failed to record command buffer", __func__);
            throw std::runtime_error("failed to record command buffer");
        }
    }
```

这里的 for 循环多次更新了 updateTextureIndex(), 但是实际上，ubo 的更新操作是实时生效的，但是 command buffer 是在 vkQueueSubmit 的时候才绘制，这导致了，ubo 的更新并不随着 mesh 的变动而变动

> 把 for 循环改到 drawFrame() 中也不行



问题出在了 record command buffer 上。

```cpp
        // update uniform buffer
        updateUniformBuffer(mCurrentFrame);

        vkResetCommandBuffer(mCommandBuffers[mCurrentFrame], 0);
        for (auto mesh : mMeshes) {
            recordCommandBuffer(mCommandBuffers[mCurrentFrame], imageIndex, mesh);
        }
```



recordCommandBuffer 只是记录命令，真正的执行命令在后面 vkQueueSubmit 的时候，所以这里使用 for 循环会导致 command buffer 只记录了最后一次的 buffer。所以只会绘制最后一次的命令。



> 使用 vkCmdUpdateBuffer() 去将更新 ubo 放在 command buffer 的操作中 ？

实际上这样操作也不太行

```cpp
for (auto& mesh : mMeshes) {
            auto& meshIndices = mesh.mIndices;
            // 需要更新 uboIndex
            spdlog::trace("{} update texture index: {}, {}", __func__, imageIndex, mesh.mMeterial_ID);
            // 由于这个 ubo 必须和 mesh 的绘制同步，所以只能通过 command buffer 的方式更新，而不使用内存映射的方式更新
            // 这会带来性能的损失
            // Todo: 但是 vkCmdUpdateBuffer() 操作必须放在 RenderPass 开始之前，所以这里这样使用也不行
            UBOIndex index;
            index.u_samplerIndex = mesh.mMeterial_ID;
            vkCmdUpdateBuffer(commandBuffer, mUBOIndexBuffers[mCurrentFrame], 0, sizeof(UBOIndex), &index);
            //updateTextureIndex(mCurrentFrame, mesh.mMeterial_ID);
            // vertex buffer
            VkBuffer vertexBuffers[] = {mVertexBuffer};
            VkDeviceSize offsets[] = {0};
            VkDeviceSize indicesOffsets = mesh.mOffset * sizeof(uint32_t);
            vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers, offsets);
            vkCmdBindIndexBuffer(commandBuffer, mIndexBuffer, indicesOffsets, VK_INDEX_TYPE_UINT32);
            vkCmdBindDescriptorSets(commandBuffer,
                VK_PIPELINE_BIND_POINT_GRAPHICS,
                mPipelineLayout,
                0,
                1,
                &mDescriptorSets[mCurrentFrame],
                0,
                nullptr
            );

            // ready to issue the draw command for the triangle
            // three vertices and draw one triangle
            //vkCmdDraw(commandBuffer, static_cast<uint32_t>(vertices.size()), 1, 0, 0);
            // Todo: 对于每一个 Shape (单独的模型) 我们都会使用不同的纹理，使用不同的 indices.
            vkCmdDrawIndexed(commandBuffer, static_cast<uint32_t>(meshIndices.size()), 1, 0, 0, 0);
        }
```



> 其他方法




