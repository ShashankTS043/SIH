# SIH
An open-source, low-cost V2V communication system using LoRa to enhance road safety. This decentralized platform provides a scalable alternative to expensive traditional ITS, making advanced safety features accessible without requiring costly infrastructure.


About The Project  
This project is an open-source implementation of a low-cost, decentralized Vehicle-to-Vehicle (V2V) communication system designed to enhance road safety. By leveraging LoRa (Long Range) technology, it provides an affordable and scalable alternative to traditional Intelligent Transportation Systems (ITS). Our goal is to make advanced safety features accessible in areas with limited cellular coverage or infrastructure, particularly in rural and developing regions.


Core Features:

• Instantaneous Crash Detection: Automatically broadcasts alerts to nearby vehicles and a cloud dashboard.

• Real-Time Congestion Monitoring: A distributed network for detecting and reporting traffic slowdowns.

• Intelligent Intersection Assist: V2V communication to prevent collisions at blind corners and junctions.

• Cloud Analytics Backend: Collects anonymized data for traffic analysis and infrastructure planning.


System Architecture  
The system consists of three main components:

On-Board Unit (OBU): An ESP32-based device with a LoRa module and GPS installed in each vehicle. It is responsible for collecting data and communicating with other vehicles.

V2V Mesh Network: A self-forming, self-healing network created by the OBUs using the LoRa protocol for direct communication.

Cloud Platform: A backend service that receives data from vehicles (when an internet connection is available via a gateway) for storage, analysis, and visualization.
