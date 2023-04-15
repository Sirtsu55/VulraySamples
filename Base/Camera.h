#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

struct Camera
{
    glm::vec3 Pos = { 0.0f, 0.0f, 0.0f };

	float FOV = 45.0f;
	float Speed = 2.5f;
	float Sensitivity = 5000.f;

    glm::vec3 Forward = { 0.0f, 0.0f, -1.0f };
    glm::vec3 Right = { 1.0f, 0.0f, 0.0f };
    glm::vec3 Up = { 0.0f, 1.0f, 0.0f };

    float Pitch = 0.0f;
    float Yaw = 0.0f;
    float Roll = 0.0f;

    float Near = 0.1f;
    float Far = 10000.0f;
    float AspectRatio = 16.0f / 9.0f;

    void MoveForward(float value)
    {
        Pos += Forward * Speed * value;
    }
    void MoveRight(float value)
    {
        Pos += Right * Speed * value;
    }

    void Rotate(float x, float y, float z)
    {
        Pitch += x * Sensitivity;
        Yaw += y * Sensitivity;
        Roll += z * Sensitivity;
    }

    glm::mat4 GetViewMatrix()
    {
        //FPS camera:  RotationX(pitch) * RotationY(yaw)
        glm::quat qPitch = glm::angleAxis(Pitch, glm::vec3(1, 0, 0));
        glm::quat qYaw = glm::angleAxis(Yaw, glm::vec3(0, 1, 0));
        glm::quat qRoll = glm::angleAxis(Roll,glm::vec3(0,0,1));  


        //For a FPS camera we can omit roll
        glm::quat orientation = qPitch * qYaw;
        orientation = glm::normalize(orientation);

        Up = glm::normalize(glm::inverse(orientation) * glm::vec3(0, 1, 0));
        Forward = glm::normalize(glm::inverse(orientation) * glm::vec3(0, 0, -1));
        Right = glm::normalize(glm::inverse(orientation) * glm::vec3(1, 0, 0));

        glm::mat4 rotation = glm::mat4_cast(orientation);
        
        glm::mat4 translate = glm::mat4(1.0f);
        translate = glm::translate(translate, -Pos);

        return rotation * translate;
    }

    glm::mat4 GetProjectionMatrix()
    {
        return glm::perspective(glm::radians(FOV), AspectRatio, Near, Far);
    }

    
};
