![Logo](https://github.com/plainFlight/plainFlightController/blob/main/assets/images/PlainFlight%20Logo%20Large.PNG)
# Overview
PlainFlight stabilisation software is for the RC pilot who wants to get the most from their model, needs to master an unstable aircraft, or to simply counteract enviromental conditions for an enjoyable flight. 

Originally created as a home project it quickly became something special with its performance, ease of build and low budget parts. These qualities led to it being refined and posted on Github for other hobbyists to have a go and enjoy. 

While PlainFlight has been designed for small electric powered model planes there is no reason to limit it to this and it could be easily modified for other small RC craft with a little effort. The code is broken down into logical modules and is well commented for those that want to understand or modify for their own purposes.

Based upon the Seeed Studio XIAO ESP32 boards and the ever popular MPU6050, it's simple to build, programmed via Arduino IDE, and cheap as chips when compared to many commercially available flight controllers.

A few different model mixes can be chosen i.e. full house plane, vtail plane, flying wing etc. As default the code will compile for a 'full house plane' with control of 4 servo and 2 motor outputs, giving controls of ailerons/flap x2, elevator, rudder and throttle x2 (differential throttle also available for twin motor options).


## Specifications:
As standard PlainFlight has the following specifications:

| Feature       | Detail        |
| ------------- | ------------- |
| Model Mixes   | Plane (aileron/flaps/elevator/rudder), Plane (aileron/flaps/V-tail), Plane (rudder/elevator), Flying Wing (elevons/rudder) |
| Flight Modes  | Pass through, gyro rate and self levelled.  |
| Actuators     | 4 servos and 2 motors (Or any combination of the 6 with modification).  |
| Actuators Refresh | 50Hz, 100Hz, 150Hz, 250Hz, 300Hz, Oneshot125 2KHz, (Or custom with modification).|
| Motors | Direct or differential thrust. |
| Radio Protocols | Sbus. |
| Battery Monitor | 1s to 3s lipo as standard, pulses throttle on low battery, cuts throttle at minimum battery voltage.|
| Failsafe | Automatic transition to self levelled mode and cuts throttle.|
| LED | Flight mode indication.|
| Target| Seeed Studio XIAO ESP32-S3, or XIAO ESP32-C3.|
| IMU| MPU6050 (GY-521 breakout board)|
| Control Loop| Stable 1KHz.|

## Hardware:

All hardware is readily avaialble from hobbyist electronics shops with the main components being:

**Seeed Studio XIAO:** [XIAO ESP32-S3](https://wiki.seeedstudio.com/xiao_esp32s3_getting_started/) or [XIAO ESP32-C3](https://wiki.seeedstudio.com/XIAO_ESP32C3_Getting_Started/)

**MPU6050:** [GY-521 Breakout Board](https://www.amazon.co.uk/MPU-6050-Accelerometer-Gyroscope-Converter-Arduino/dp/B0BZXT477Z/ref=sr_1_7?crid=1PUDPKVVKYGMW&keywords=gy-521%2Bmpu6050%2Bimu&qid=1700420083&sprefix=GY-521%2Caps%2C316&sr=8-7&th=1)

The following wiring diagram details how to assemble the flight controller from component parts, but for full details on wiring and setup please see instructions/directions manual:

![Wiring diagram](https://github.com/plainFlight/plainFlightController/blob/main/assets/images/plainFlight%20Controller%20Connection%20Diagram.png)

## Disclaimer:

Do not expect this software to out perform other more established flight controller projects such as ArduPilot, inav, betaFlight etc. This code shall be considered as highly experimental and is not designed or written to any safety critical, or mission critical standards. It is given/shared for free with the knowledge and understanding that this open source flight controller software is only for small hobby based electrically powered model aircraft, or other small hobby radio controlled vehicles. It is intended to be used or modified to suit your needs for small models and is NOT to be used on any manned vehicles. The author(s) shall not be held responsible or accountable for any damage, injury or loss that may be inflicted or incurred as a result of the use or missuse of this code. Use and modify at your own risk and use within accordance of your countrys laws and/or regulations. 

To put it more bluntly:

THIS SOFTWARE IS PROVIDED BY THE CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
