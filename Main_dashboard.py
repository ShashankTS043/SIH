from flask import Flask, render_template, request
from backend import function
import os
import pandas as pd
import plotly.express as px
import plotly.io as pio
import folium
from folium.plugins import HeatMap

app = Flask(__name__)

CSV_INTERSECTION = "traffic_data.csv"
CSV_CONGESTION = "traffic_log.csv"

@app.route("/")
def index():
    return render_template("index.html")


@app.route("/report")
def report():
    intersection_data, congestion_data = None, None
    intersection_plot, congestion_plot = None, None
    sample_intersection, sample_congestion = None, None
    has_intersection, has_congestion = False, False

    if os.path.exists(CSV_INTERSECTION):
        df = pd.read_csv(CSV_INTERSECTION)
        if not df.empty:
            has_intersection = True
            sample_intersection = df.head(10).iloc[:, 1:]
            sample_intersection.columns = [" ", " "]
            fig = px.bar(df, x="step", y="speed_mps", title="Intersection Speeds", color_discrete_sequence=["#2253a3"])
            fig.update_layout(
                plot_bgcolor="white",      # background inside the plot
                paper_bgcolor="rgba(0,0,0,0)",  # background outside (transparent here)
                font=dict(color="white"),
            )
            intersection_plot = pio.to_html(fig, full_html=False)
        intersection_data = df

    if os.path.exists(CSV_CONGESTION):
        df2 = pd.read_csv(CSV_CONGESTION)
        if not df2.empty:
            has_congestion = True
            sample_congestion = df2.head(10).iloc[:, 1:]
            sample_congestion.columns = [" ", " "]
            fig2 = px.bar(df2, x="Step", y="Speed", title="Congestion Speeds", color_discrete_sequence=["#2253a3"])
            fig2.update_layout(
                plot_bgcolor="white",      # background inside the plot
                paper_bgcolor="rgba(0,0,0,0)",  # background outside (transparent here)
                font=dict(color="white"),
            )
            congestion_plot = pio.to_html(fig2, full_html=False)
        congestion_data = df2

    return render_template(
        "report.html",
        has_intersection=has_intersection,
        has_congestion=has_congestion,
        sample_intersection=sample_intersection,
        sample_congestion=sample_congestion,
        intersection_plot=intersection_plot,
        congestion_plot=congestion_plot
    )


@app.route("/map")
def map_view():
    incident_df = function.fetch_incident_data()
    folium_map = None

    if not incident_df.empty:
        center_lat = incident_df['lat'].mean()
        center_lon = incident_df['lon'].mean()
        folium_map = folium.Map(location=[center_lat, center_lon], zoom_start=12)

        for _, row in incident_df[0:5].iterrows():
            if _>5:
                break
            folium.CircleMarker(
                location=[row['lat'], row['lon']],
                radius=6,
                color="red",
                fill=True,
                fill_color="red",
                popup=f"Incident ID: {row.get('id', 'N/A')}"
            ).add_to(folium_map)

        folium_map = folium_map._repr_html_()

    return render_template("map.html", incident_df=incident_df, folium_map=folium_map)



@app.route("/heat")
def heat():
    eps = float(request.args.get("eps", 0.5))
    min_samples = int(request.args.get("min_samples", 2))

    raw_data = function.fetch_live_data()
    clustered_data = function.run_dbscan(raw_data, eps, min_samples)

    heatmap_html = None
    if not clustered_data.empty:
        center_lat = clustered_data['Latitude'].mean()
        center_lon = clustered_data['Longitude'].mean()

        m = folium.Map(location=[center_lat, center_lon], zoom_start=10)
        heatmap_data = clustered_data[clustered_data['cluster'] != -1][['Latitude', 'Longitude']].values.tolist()
        if heatmap_data:
            HeatMap(heatmap_data, radius=20).add_to(m)

        heatmap_html = m._repr_html_()

    return render_template(
        "heatmap.html",
        heatmap_html=heatmap_html,
        incidents=clustered_data.to_dict(orient="records"),
        eps=eps,
        min_samples=min_samples
    )



if __name__ == "__main__":
    app.run(host="0.0.0.0", port=5000)
